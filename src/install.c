/* install.c - download + per-OS install logic
 *
 * Install methods:
 *   windows: .exe -> run installer silently where known, else copy to bin
 *            .msi -> msiexec /i /qn
 *            .zip/.7z -> extract into install dir
 *   linux:   .deb -> dpkg -i      .rpm -> rpm -Uvh
 *            .appimage -> chmod +x, copy to bin
 *            .tar.gz/.tgz/.tar.xz -> extract into install dir
 *   macos:   .dmg -> hdiutil attach, copy .app to /Applications, detach
 *            .pkg -> installer -pkg
 *            .tar.gz/.tgz -> extract into install dir
 */
#include "install.h"
#include "pmm.h"
#include "out.h"
#include "i18n.h"
#include "http.h"
#include "json.h"
#include "sha256.h"
#include "sha1.h"
#include "mirrors.h"
#include "pdm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define PMM_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>          /* unlink/symlink (cpio unpacker) */
#include <dirent.h>          /* opendir/readdir (empty-stage check) */
#define PMM_MKDIR(p) mkdir((p), 0755)
#endif

static int has_suffix(const char *s, const char *suf) {
    size_t sl = strlen(s), xl = strlen(suf);
    return sl >= xl && strcasecmp(s + sl - xl, suf) == 0;
}

static const char *base_name(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *p = (a > b) ? a : b;
    return p ? p + 1 : path;
}

static int run_cmd_quiet(const char *cmd) {
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}

/* Strip "<algo> *path" style prefix from checksum files; keep last token's hex. */
static void extract_hex(const char *text, const char *filename, char *hex, size_t hexsize) {
    hex[0] = '\0';
    const char *line = text;
    while (line && *line) {
        const char *eol = strchr(line, '\n');
        size_t ll = eol ? (size_t)(eol - line) : strlen(line);
        char buf[1024];
        if (ll >= sizeof(buf)) ll = sizeof(buf) - 1;
        memcpy(buf, line, ll);
        buf[ll] = '\0';
        /* trim */
        char *s = buf;
        while (*s == ' ' || *s == '\t' || *s == '\r') s++;
        size_t sl = strlen(s);
        while (sl && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) s[--sl] = '\0';
        /* does this line mention the file (or is a bare hash)? */
        char *hend = s;
        while (*hend && *hend != ' ' && *hend != '\t' && *hend != '*') hend++;
        int is_hex_prefix = 1;
        for (char *p = s; p < hend; p++)
            if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
                { is_hex_prefix = 0; break; }
        if (is_hex_prefix && hend > s) {
            /* bare "<hex>" or "<hex> *file" */
            if (!*hend || strstr(hend, filename)) {
                snprintf(hex, hexsize, "%.*s", (int)(hend - s), s);
                return;
            }
        } else if (strstr(s, filename)) {
            /* "hash  file" reversed order unlikely; take first token */
            char tok[256]; size_t t = 0;
            while (s[t] && s[t] != ' ' && s[t] != '\t' && t < sizeof(tok) - 1) { tok[t] = s[t]; t++; }
            tok[t] = '\0';
            int ok = *tok;
            for (char *p = tok; *p; p++)
                if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
                    { ok = 0; break; }
            if (ok) { snprintf(hex, hexsize, "%s", tok); return; }
        }
        line = eol ? eol + 1 : NULL;
    }
}

/* Best-effort sha256/sha1 verification against sidecar checksum files
 * (<url>.sha256 / <url>.sha1). Returns 0 on ok-or-no-checksum, -1 on mismatch. */
static int verify_checksums(const char *url, const char *path, const char *name) {
    char hex[128];
    struct { const char *ext; int algo; } algos[] = {
        { ".sha256", 256 }, { ".sha1", 1 }, { NULL, 0 }
    };
    for (int i = 0; algos[i].ext; i++) {
        char csum_url[2400];
        snprintf(csum_url, sizeof(csum_url), "%s%s", url, algos[i].ext);
        int status = 0;
        char *text = http_get(csum_url, &status);
        if (!text || (status != 200 && status != 0)) { free(text); continue; }
        if (algos[i].algo == 256) {
            if (pmm_sha256_file(path, hex) != 0) { free(text); return -1; }
        } else {
            if (pmm_sha1_file(path, hex) != 0) { free(text); return -1; }
        }
        char expect[128];
        extract_hex(text, name, expect, sizeof(expect));
        free(text);
        if (!expect[0]) {
            pmm_warn("%s", pmm_tr_fmt("msg.warn.parse-checksum", algos[i].ext));
            continue;
        }
        /* case-insensitive compare */
        if (strcasecmp(expect, hex) != 0) {
            pmm_error("%s", pmm_tr_fmt("msg.checksum-mismatch", algos[i].ext, expect, hex));
            return -1;
        }
        pmm_success("%s", pmm_tr_fmt("msg.checksum-ok", algos[i].algo == 256 ? "sha256" : "sha1", hex));
    }
    return 0;
}

/* A bare (no-dot) release asset on Unix must actually be usable on this
 * platform — e.g. fzf publishes a bare 'fzf' for Android, which must not be
 * installed on Linux. Uses `file`; be permissive where it isn't available. */
static int native_binary_ok(const char *path, PmmOS os) {
#ifdef _WIN32
    (void)path; (void)os; return 1;   /* `file` isn't present on Windows */
#else
    if (os != OS_LINUX && os != OS_MACOS) return 1;
    char cmd[1400], out[512] = "";
    snprintf(cmd, sizeof(cmd), "file -b \"%s\" 2>/dev/null", path);
    FILE *f = popen(cmd, "r");
    if (!f) return 1;
    if (fgets(out, sizeof(out), f) == NULL) out[0] = 0;
    pclose(f);
    if (!out[0]) return 1;                     /* `file` missing: allow */
    if (strstr(out, "Android") || strstr(out, "for Android")) return 0;  /* android ELF */
    if (strstr(out, "PE32")) return 0;         /* Windows binary */
    if (strstr(out, "Mach-O")) return 0;       /* macOS binary */
    return 1;
#endif
}

static int install_path(const char *path, const char *name);

int pmm_no_cache = 0;   /* --no-cache: drop cached file before download */
int pmm_force_reinstall = 0;   /* --force: reinstall even if already present */
int pmm_yes = 0;   /* -y/--yes: skip confirmation prompts */

/* Extract the data.tar.* member of a .deb (an ar container) to `outdata`.
 * Pure C (no ar/dpkg required). Returns compression code:
 *   0=gz 1=zst 2=xz 3=bz2, or -1 on failure. Caller unpacks with `tar`. */
static int deb_extract_data(const char *deb, const char *outdata) {
    FILE *f = fopen(deb, "rb");
    if (!f) return -1;
    char magic[8];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "!<arch>\n", 8) != 0) { fclose(f); return -1; }
    char hdr[60];
    while (fread(hdr, 1, 60, f) == 60) {
        char name[17];
        memcpy(name, hdr, 14); name[14] = 0;   /* member name without trailing / and .tar.<ext> handled below */
        char *slash = strchr(name, '/');
        if (slash) *slash = 0;
        size_t sz = (size_t)strtoul(hdr + 48, NULL, 10);
        int comp = -1;
        if (strncmp(name, "data.tar.gz", 11) == 0) comp = 0;
        else if (strncmp(name, "data.tar.zst", 12) == 0) comp = 1;
        else if (strncmp(name, "data.tar.xz", 11) == 0) comp = 2;
        else if (strncmp(name, "data.tar.bz2", 12) == 0) comp = 3;
        if (comp >= 0) {
            FILE *o = fopen(outdata, "wb");
            if (!o) { fclose(f); return -1; }
            char buf[65536]; size_t left = sz;
            while (left) {
                size_t n = fread(buf, 1, left > sizeof(buf) ? sizeof(buf) : left, f);
                if (!n) break;
                if (fwrite(buf, 1, n, o) != n) break;
                left -= n;
            }
            fclose(o); fclose(f);
            return left == 0 ? comp : -1;
        }
        fseek(f, (long)sz + (sz % 2), SEEK_CUR);
    }
    fclose(f);
    return -1;
}

static unsigned rpm_be32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | (unsigned)p[3];
}

/* Extract the payload (gzip/zstd'd cpio) of an RPM to `out`. Pure C header walk
 * (lead -> signature header -> main header), no rpm/rpm2cpio required. */
static int rpm_extract_payload(const char *rpm, const char *out) {
    FILE *f = fopen(rpm, "rb");
    if (!f) return -1;
    unsigned char lead[96];
    if (fread(lead, 1, 96, f) != 96 || !(lead[0]==0xed && lead[1]==0xab && lead[2]==0xee && lead[3]==0xdb)) {
        fclose(f); return -1;
    }
    /* signature header (rpm header struct): 16 bytes + index + data, aligned to 8 */
    unsigned char sh[16];
    if (fread(sh, 1, 16, f) != 16) { fclose(f); return -1; }
    unsigned sh_ni = rpm_be32(sh + 8), sh_hs = rpm_be32(sh + 12);
    long sig_total = (long)(16 + sh_ni*16 + sh_hs);
    sig_total = (sig_total + 7) & ~7L;
    fseek(f, 96 + sig_total, SEEK_SET);
    /* main header */
    unsigned char mh[16];
    if (fread(mh, 1, 16, f) != 16 ||
        !(mh[0]==0x8e && mh[1]==0xad && mh[2]==0xe8 && mh[3]==0x01)) { fclose(f); return -1; }
    unsigned mh_ni = rpm_be32(mh + 8), mh_hs = rpm_be32(mh + 12);
    long payload = ftell(f) + (long)mh_ni*16 + (long)mh_hs;
    fseek(f, payload, SEEK_SET);
    FILE *o = fopen(out, "wb");
    if (!o) { fclose(f); return -1; }
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        if (fwrite(buf, 1, n, o) != n) break;
    fclose(o); fclose(f);
    return 0;
}

/* Detect the compression codec of an RPM payload so we can pick the right
 * decompressor without assuming gzip. Returns:
 *   0=gzip 1=zstd 2=xz 3=bzip2 4=lzma, or -1 if unrecognized. */
static int rpm_payload_compress(const char *payload) {
    FILE *f = fopen(payload, "rb");
    if (!f) return -1;
    unsigned char m[8];
    size_t n = fread(m, 1, 8, f);
    fclose(f);
    if (n >= 2 && m[0] == 0x1f && m[1] == 0x8b) return 0;                          /* gzip */
    if (n >= 4 && m[0] == 0x28 && m[1] == 0xb5 && m[2] == 0x2f && m[3] == 0xfd) return 1; /* zstd */
    if (n >= 6 && m[0] == 0xfd && m[1] == 0x37 && m[2] == 0x7a && m[3] == 0x58 &&
        m[4] == 0x5a && m[5] == 0x00) return 2;                                    /* xz */
    if (n >= 3 && m[0] == 0x42 && m[1] == 0x5a && m[2] == 0x68) return 3;          /* bzip2 */
    if (n >= 5 && m[0] == 0x5d && m[1] == 0x00 && m[2] == 0x00 && m[3] == 0x00 && m[4] == 0x80) return 4; /* lzma */
    return -1;
}

#ifndef _WIN32
/* ---------- pure-C cpio unpacker (no system cpio required) ----------
 * Linux-only: used by the .rpm branch (os == OS_LINUX). It relies on POSIX
 * mkdir(2)/symlink/S_ISREG etc., which mingw doesn't provide. Windows never
 * enters the rpm path, so this whole block is compiled out there.
 *
 * Reads a cpio archive from FILE* in (typically a popen decompression pipe)
 * and writes paths relative to dest. Handles newc (070701), newc-crc (070702)
 * and old odc (070707). Archive names are always kept under dest ('.' and '/'
 * are stripped; '..' names are skipped). Returns 0 on success. */

static unsigned c_hexn(const char *s, int n) {
    unsigned v = 0; int i;
    for (i = 0; i < n; i++) { char ch = s[i]; v <<= 4;
        if (ch >= '0' && ch <= '9') v |= (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') v |= (unsigned)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') v |= (unsigned)(ch - 'A' + 10); }
    return v;
}

/* Create every directory component under the (final) path. */
static void cpio_mkdirs(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') { char c = *p; *p = '\0'; mkdir(path, 0755); *p = c; }
    }
}

/* Skip exactly n bytes from a possibly non-seekable stream. */
static void cpio_skip(FILE *in, unsigned long n) {
    char b[2048];
    while (n) { unsigned long c = n > sizeof(b) ? sizeof(b) : n;
        size_t g = fread(b, 1, c, in); if (!g) break; n -= g; }
}

int pmm_cpio_unpack(const char *dest, FILE *in) {
    char name[8192];
    int first = 1, is_odc = 0;

    for (;;) {
        /* read the 6-byte magic */
        char hdr[110];
        if (fread(hdr, 1, 6, in) != 6) break;             /* EOF -> done (no trailer) */
        if (!(memcmp(hdr,"070701",6)==0 || memcmp(hdr,"070702",6)==0 ||
              memcmp(hdr,"070707",6)==0)) break;          /* not a cpio magic -> done */
        if (first) { is_odc = (memcmp(hdr,"070707",6)==0); first = 0; }
        int hlen;
        if (is_odc) { hlen = 76; if (fread(hdr+6,1,70,in)!=70) return -1; }
        else        { hlen = 110; if (fread(hdr+6,1,104,in)!=104) return -1; }

        unsigned mode, data_size, namesize;
        if (is_odc) {
            mode       = c_hexn(hdr+6,  6);
            data_size  = c_hexn(hdr+6+30, 6);
            namesize   = 0;   /* odc name is NUL-terminated */
        } else {
            /* newc (070701) / crc (070702): fields are 8-hex-char ASCII words
             * that follow the 6-byte magic directly:
             *   ino(6) mode(14) uid(22) gid(30) nlink(38) mtime(46) filesize(54)
             *   devmaj(62) devmin(70) rdevmaj(78) rdevmin(86) name(94) check(102)
             * (offsets are relative to hdr; magic occupies hdr[0..5]). */
            mode       = c_hexn(hdr+14, 8);
            data_size  = c_hexn(hdr+54, 8);
            namesize   = c_hexn(hdr+94, 8);
        }

        if (is_odc) {
            /* NUL-terminated name of unbounded length */
            int pos = 0;
            for (;;) {
                int ch = fgetc(in);
                if (ch == EOF) return -1;
                if (pos < (int)sizeof(name)-1) name[pos++] = (char)ch;
                if (ch == 0) break;
            }
            name[pos] = 0;
        } else {
            if (namesize == 0 || namesize >= sizeof(name)) namesize = (unsigned)sizeof(name)-1;
            if (fread(name, 1, namesize, in) != namesize) return -1;
            name[namesize] = 0;
            /* align to 4 on the (header+name) boundary */
            unsigned padh = (unsigned)(4 - (((unsigned)hlen + namesize) & 3)) & 3;
            cpio_skip(in, padh);
        }

        if (name[0]==0 || strcmp(name,"TRAILER!!!")==0) break;   /* end of archive */

        /* make safe relative path */
        const char *rel = name;
        while (*rel=='/') rel++;
        while (rel[0]=='.' && rel[1]=='/') rel += 2;
        if (strncmp(rel,"../",3)==0 || strstr(rel,"/../") || strcmp(rel,"..")==0) {
            cpio_skip(in, data_size);                         /* refuse .. traversal */
            if (!is_odc) cpio_skip(in, (4-(data_size&3))&3);
            continue;
        }

        char full[8400];
        snprintf(full, sizeof(full), "%s/%s", dest, rel);
        unsigned ft = mode & 0170000;

        if (S_ISREG(ft)) {
            cpio_mkdirs(full);
            FILE *of = fopen(full, "wb");
            if (of) {
                char b[65536]; unsigned long left = data_size;
                while (left) { unsigned long c = left > sizeof(b) ? sizeof(b) : left;
                    size_t g = fread(b,1,c,in); if (!g) break; fwrite(b,1,g,of); left -= g; }
                fclose(of);
                chmod(full, mode & 07777);
            } else cpio_skip(in, data_size);
        } else if (S_ISDIR(ft)) {
            cpio_mkdirs(full); mkdir(full, mode & 07777);
        } else if (S_ISLNK(ft)) {
            char t[4096];
            if (data_size < sizeof(t)-1) {
                if (fread(t,1,data_size,in)!=data_size) return -1;
                t[data_size]=0; cpio_mkdirs(full); unlink(full);
                if (symlink(t, full) != 0) { /* best-effort symlink */ }
            } else cpio_skip(in, data_size);
        } else {
            cpio_skip(in, data_size);                          /* device/fifo/unknown */
        }

        if (!is_odc) cpio_skip(in, (4-(data_size&3))&3);       /* data->4 pad */
    }
    return 0;
}

/* Run `decompcmd` (e.g. "gzip -dc file") and unpack its cpio output into dest.
 * Needs decompcmd to emit a cpio stream on stdout. Returns 0 on success. */
/* True if `dir` exists and contains no entries (best-effort). */
static int dir_is_empty(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;                     /* missing dir is NOT "empty ok" */
    int empty = 1; struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        empty = 0; break;
    }
    closedir(d);
    return empty;
}

static int pmm_cpio_unpack_cmd(const char *dest, const char *decompcmd) {
    FILE *p = popen(decompcmd, "r");
    if (!p) return -1;
    int rc = pmm_cpio_unpack(dest, p);
    int prc = pclose(p);
    if (rc != 0) return -1;
    if (prc != 0) return -1;
    /* Never report success if the unpacked stream produced nothing — otherwise
     * the caller would sudo-copy an empty dir and "install" nothing. */
    if (dir_is_empty(dest)) return -1;
    return 0;
}
#endif /* !_WIN32 */   /* end pure-C cpio unpacker */

int install_file(const char *url, const char *name) {
    char cache[1024], path[1200];
    pmm_cache_dir(cache, sizeof(cache));
    snprintf(path, sizeof(path), "%s/%s", cache, name);
    if (pmm_no_cache) remove(path);   /* force a fresh download */

    /* apt-style fallback: mirrors (by priority) first, then the origin URL */
    MirrorList *ml = mirrors_load();
    int ncand = 0;
    char **cands = mirrors_download_candidates(ml, url, &ncand);
    int ok = -1;
    for (int i = 0; i < ncand; i++) {
        pmm_info("%s", pmm_tr_fmt("msg.downloading", cands[i],
               i < ncand - 1 ? " (mirror)" : ""));
        if (http_download(cands[i], path) == 0) { ok = 0; break; }
        pmm_warn("%s", pmm_tr("msg.warn.download-retry"));
        remove(path);
    }
    for (int i = 0; i < ncand; i++) free(cands[i]);
    free(cands);
    mirrors_free(ml);
    if (ok != 0) {
        pmm_error("%s", pmm_tr_fmt("msg.err.download-failed", url));
        return -1;
    }
    pmm_success("%s", pmm_tr_fmt("msg.downloaded", path));

    /* integrity: sha256 + sha1, verified against sidecar checksums when present */
    char hex[128];
    if (pmm_sha256_file(path, hex) == 0)
        pmm_info("%s", pmm_tr_fmt("msg.hash-sha256", hex));
    if (pmm_sha1_file(path, hex) == 0)
        pmm_info("%s", pmm_tr_fmt("msg.hash-sha1", hex));
    if (verify_checksums(url, path, name) != 0) {
        pmm_error(pmm_tr("msg.err.checksum-refuse"));
        remove(path);
        return -1;
    }

    return install_path(path, name);
}

/* Install an already-local file path by its extension. Shared by downloads
 * (install_file) and local `pmm install -dpkg/-msi <file>` / `install file.deb`.
 * Returns 0 on success. */
static int install_path(const char *path, const char *name) {
    PmmOS os = pmm_detect_os();
    char dest[1024];
    pmm_install_dir(dest, sizeof(dest));

    const char *bname = base_name(name);
    /* A bare binary (no extension) must really be a native executable for this
     * OS; otherwise refuse so the caller can fall back to the os-named asset. */
    if (strchr(bname, '.') == NULL) {
        pmm_info("%s", pmm_tr_fmt("msg.verifying-native", bname, pmm_os_name(os)));
        if (!native_binary_ok(path, os)) {
            pmm_warn("%s", pmm_tr_fmt("msg.warn.not-usable", bname, pmm_os_name(os)));
            return -1;
        }
    }

    /* forward-slash clone of the path (native exes/tools accept it and the
     * git-bash shell won't mangle a command word like "C:/...") */
    char pwin[1200];
    snprintf(pwin, sizeof(pwin), "%s", path);
    for (char *q = pwin; *q; q++) if (*q == '\\') *q = '/';
    char cmd[2600];

    /* .pdm packages are installed through the deb-like manager */
    if (has_suffix(bname, ".pdm") || has_suffix(bname, ".PDM")) {
        extern int pdm_install_file(const char *path);
        pmm_info("%s", pmm_tr_fmt("msg.installing", bname));
        if (pdm_install_file(path) != 0) {
            pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", bname));
            return -1;
        }
        pmm_success("%s", pmm_tr_fmt("msg.installed", bname));
        pmm_add_to_path();
        return 0;
    }

    if (has_suffix(bname, ".deb") && os == OS_LINUX) {
        /* Self-extract in C (no ar/dpkg needed): pull data.tar.gz, tar it out */
        char cache[1024], fdata[1300], fstage[1300];
        pmm_cache_dir(cache, sizeof(cache));
        snprintf(fdata, sizeof(fdata), "%s/.pmm-deb-data.tar.gz", cache);
        snprintf(fstage, sizeof(fstage), "%s/.pmm-deb-stage", cache);
        int dc = deb_extract_data(path, fdata);
        if (dc >= 0) {
            const char *tflag = dc == 1 ? "--zstd -x" : dc == 2 ? "-xJ" : dc == 3 ? "-xj" : "-xz";
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && tar %sf \"%s\" -C \"%s\" "
                "&& sudo cp -a \"%s\"/. / && rm -rf \"%s\" \"%s\"",
                fstage, fstage, tflag, fdata, fstage, fstage, fstage, fdata);
        } else {
            snprintf(cmd, sizeof(cmd),
                "if command -v dpkg >/dev/null 2>&1; then sudo dpkg -i \"%s\"; "
                "elif command -v ar >/dev/null 2>&1; then T=$(mktemp -d); "
                "(cd \"$T\" && ar p \"%s\" data.tar.gz | tar -xzf -); sudo cp -a \"$T\"/. /; rm -rf \"$T\"; "
                "elif command -v alien >/dev/null 2>&1; then sudo alien -i \"%s\"; "
                "else echo -e \"\\033[31m[PMM]:[ERROR]cannot unpack .deb (need tar)\\033[0m\" 1>&2; exit 1; fi",
                path, path, path);
        }
    } else if (has_suffix(bname, ".rpm") && os == OS_LINUX) {
        /* Self-extract in C (no rpm/rpm2cpio): pull cpio payload, decompress,
         * unpack into a cache dir, then sudo-copy to /.  The decompressor is
         * chosen from the payload's real codec (NOT assumed gzip -- RHEL9+ and
         * Fedora ship zstd, some srpms use xz/lzma).  `set -e` + each stage
         * chained with && means a failed decompress aborts and returns non-zero
         * instead of pmm falsely reporting success. */
        char cache[1024], fdata[1300], fstage[1300];
        pmm_cache_dir(cache, sizeof(cache));
        snprintf(fdata, sizeof(fdata), "%s/.pmm-rpm-payload", cache);
        snprintf(fstage, sizeof(fstage), "%s/.pmm-rpm-stage", cache);
        if (rpm_extract_payload(path, fdata) == 0) {
            /* Decompress with pmm's own pure-C cpio unpacker so no system
             * cpio/rpm/rpm2cpio/alien is required. We only need ONE of
             * gzip/zstd/xz/bzip2/lzma on the host (virtually every Linux has at
             * least one). The decompressor is chosen from the payload's real
             * codec -- gzip is NOT assumed (RHEL9+/Fedora ship zstd; some srpms
             * use xz/lzma). */
            int ct = rpm_payload_compress(fdata);
            const char *dc = (ct == 1) ? "zstd" :
                             (ct == 2) ? "xz" :
                             (ct == 3) ? "bzip2" :
                             (ct == 4) ? "lzma" : "gzip";
            if (getenv("PMM_DEBUG"))
                fprintf(stderr, "[pmm-debug] rpm codec=%d dc=%s fdata=%s fstage=%s\n",
                        ct, dc, fdata, fstage);
            (void)dc;
            char dcmd[1500];
            snprintf(dcmd, sizeof(dcmd), "%s -dc \"%s\"", dc, fdata);
            snprintf(cmd, sizeof(cmd),
                "sudo cp -a \"%s\"/. / && rm -rf \"%s\" \"%s\"",
                fstage, fstage, fdata);
#ifdef _WIN32
            /* Windows never reaches this Linux branch at runtime, but the code
             * is still compiled. Fall back to the shell cpio path so the build
             * doesn't fail on the missing pure-C unpacker. */
            snprintf(cmd, sizeof(cmd),
                "if command -v rpm >/dev/null 2>&1; then sudo rpm -Uvh \"%s\"; "
                "elif command -v rpm2cpio >/dev/null 2>&1; then sudo rpm2cpio \"%s\" | (cd / && sudo cpio -idm --quiet); "
                "elif command -v alien >/dev/null 2>&1; then sudo alien -i \"%s\"; "
                "else echo -e \"\\033[31m[PMM]:[ERROR]cannot unpack .rpm (need rpm/cpio)\\033[0m\" 1>&2; exit 1; fi",
                path, path, path);
            if (system(cmd) != 0) { pmm_error(pmm_tr("msg.err.failed-install")); return -1; }
            (void)dcmd; (void)ct;
#else
            /* create the (possibly multi-level) stage dir first; pmm_cpio_unpack
             * only mkdir's the per-file parent components, not the root itself,
             * so without this fopen("stage/usr/...") fails and data is skipped. */
            {
                char mkstag[1400];
                snprintf(mkstag, sizeof(mkstag), "mkdir -p \"%s\"", fstage);
                if (system(mkstag) != 0) {
                    pmm_error("%s", pmm_tr_fmt("msg.err.rpm-stage", fstage));
                    return -1;
                }
            }
            /* unpack into stage (no sudo needed; stage is user-writable) */
            if (pmm_cpio_unpack_cmd(fstage, dcmd) != 0) {
                pmm_error("%s", pmm_tr_fmt("msg.err.rpm-decompress", dc));
                return -1;
            }
            /* copy the unpacked payload into /, then clean up */
            if (system(cmd) != 0) {
                pmm_error(pmm_tr("msg.err.rpm-copy"));
                return -1;
            }
            (void)ct;
#endif
        } else {
            snprintf(cmd, sizeof(cmd),
                "if command -v rpm >/dev/null 2>&1; then sudo rpm -Uvh \"%s\"; "
                "elif command -v rpm2cpio >/dev/null 2>&1; then sudo rpm2cpio \"%s\" | (cd / && sudo cpio -idm --quiet); "
                "elif command -v alien >/dev/null 2>&1; then sudo alien -i \"%s\"; "
                "else echo -e \"\\033[31m[PMM]:[ERROR]cannot unpack .rpm (need gzip/cpio)\\033[0m\" 1>&2; exit 1; fi",
                path, path, path);
        }
    } else if (has_suffix(bname, ".apk") && os == OS_LINUX) {
        /* Alpine: apk add if available, else extract */
        snprintf(cmd, sizeof(cmd),
                 "if command -v apk >/dev/null 2>&1; then sudo apk add --allow-untrusted \"%s\"; "
                 "else tar -xf \"%s\" -C \"%s\"; fi", path, path, dest);
    } else if (has_suffix(bname, ".pkg.tar.zst") && os == OS_LINUX) {
        /* Arch: pacman -U if available, else extract */
        snprintf(cmd, sizeof(cmd),
                 "if command -v pacman >/dev/null 2>&1; then sudo pacman -U --noconfirm \"%s\"; "
                 "else tar --zstd -xf \"%s\" -C \"%s\"; fi", path, path, dest);
    } else if (has_suffix(bname, ".appimage") && os == OS_LINUX) {
        snprintf(cmd, sizeof(cmd), "chmod +x \"%s\" && mv -f \"%s\" \"%s/\"", path, path, dest);
    } else if ((has_suffix(bname, ".tar.gz") || has_suffix(bname, ".tgz") ||
                has_suffix(bname, ".tar.xz") || has_suffix(bname, ".tar.bz2") ||
                has_suffix(bname, ".zip") || has_suffix(bname, ".7z"))) {
        if (has_suffix(bname, ".zip"))
            snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", path, dest);
        else if (has_suffix(bname, ".7z"))
            snprintf(cmd, sizeof(cmd), "7z x -y -o\"%s\" \"%s\"", dest, path);
        else
            snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", path, dest);
    } else if (has_suffix(bname, ".dmg") && os == OS_MACOS) {
        snprintf(cmd, sizeof(cmd),
                 "h=$(hdiutil attach \"%s\" | grep -o '/Volumes/.*' | head -1); "
                 "sudo cp -R \"$h\"/*.app /Applications/ 2>/dev/null; hdiutil detach \"$h\"",
                 path);
    } else if (has_suffix(bname, ".pkg") && os == OS_MACOS) {
        snprintf(cmd, sizeof(cmd), "sudo installer -pkg \"%s\" -target /", path);
    } else if (has_suffix(bname, ".msi") && os == OS_WINDOWS) {
        /* silent install; note system() runs cmd.exe on Windows, so use single
         * slashes and a forward-slash path (cmd accepts both) */
        snprintf(cmd, sizeof(cmd), "msiexec /i \"%s\" /qn", pwin);
    } else if (has_suffix(bname, ".exe") && os == OS_WINDOWS) {
        /* auto-run the installer. system() runs cmd.exe, which chokes on shell
         * control operators (|| / &) here, so issue ONE silent invocation
         * (common silent flags; vendors ignore the ones they don't know). */
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" /S /VERYSILENT /quiet --silent 2>nul", pwin);
    } else {
        /* unknown type for this OS: copy the single file into the install dir;
         * on Linux/macOS make it executable (bare ELF/script releases). */
        if (os == OS_LINUX || os == OS_MACOS)
            snprintf(cmd, sizeof(cmd),
                     "cp -f \"%s\" \"%s/\" 2>/dev/null && chmod +x \"%s/%s\" 2>/dev/null || true",
                     path, dest, dest, bname);
        else
            snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s/\" 2>/dev/null || true", path, dest);
    }

    pmm_info("%s", pmm_tr_fmt("msg.installing", bname));
    if (run_cmd_quiet(cmd) != 0) {
        pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", cmd));
        return -1;
    }
    pmm_success("%s", pmm_tr_fmt("msg.installed", bname));
    pmm_add_to_path();
    return 0;
}

/* Install a local file by its extension — e.g. `pmm install -dpkg foo.deb`
 * (Linux) or `pmm install -msi foo.msi` (Windows). Returns 0 on success. */
int install_local_file(const char *path) {
    if (!path || !*path) { pmm_error(pmm_tr("msg.err.empty-path")); return -1; }
    FILE *chk = fopen(path, "rb");
    if (!chk) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-open", path)); return -1; }
    fclose(chk);
    pmm_info("%s", pmm_tr_fmt("msg.installing-file", path));
    return install_path(path, path);
}

/* ---------- python-style version selection (==,>=,<=,>,<,!= , comma list) ---------- */

/* Compare dotted version strings numerically ("24.20.0" vs "24.2.0"). */
static int vcmp(const char *a, const char *b) {
    const char *pa = a, *pb = b;
    while (*pa || *pb) {
        double va = 0, vb = 0; int ha = 0, hb = 0;
        while (*pa && *pa != '.' && *pa != '-') { va = va * 10 + (*pa - '0'); pa++; ha = 1; }
        while (*pb && *pb != '.' && *pb != '-') { vb = vb * 10 + (*pb - '0'); pb++; hb = 1; }
        if (ha != hb) return ha < hb ? -1 : 1;
        if (va != vb) return va < vb ? -1 : 1;
        if (*pa == '.') pa++; if (*pb == '.') pb++;
    }
    return 0;
}

/* Does `ver` satisfy one `cond` like ">=24.20.0"? (bare "24.20.0" means ==) */
static int cond_match(const char *ver, const char *cond) {
    while (*cond == ' ' || *cond == '\t') cond++;
    char op[3] = {0}; int oi = 0;
    if (cond[0] == '=' && cond[1] == '=') { op[0] = '='; op[1] = '='; oi = 2; }
    else if (cond[0] == '!' && cond[1] == '=') { op[0] = '!'; op[1] = '='; oi = 2; }
    else if (cond[0] == '>' && cond[1] == '=') { op[0] = '>'; op[1] = '='; oi = 2; }
    else if (cond[0] == '<' && cond[1] == '=') { op[0] = '<'; op[1] = '='; oi = 2; }
    else if (cond[0] == '>') { op[0] = '>'; oi = 1; }
    else if (cond[0] == '<') { op[0] = '<'; oi = 1; }
    else { op[0] = '='; op[1] = '='; oi = 2; } /* bare version == */
    const char *num = cond + oi;
    while (*num == ' ' || *num == '\t') num++;
    int c = vcmp(ver, num);
    if (oi == 2 && op[0] == '=' && op[1] == '=') return c == 0;
    if (oi == 2 && op[0] == '!' && op[1] == '=') return c != 0;
    if (oi == 2 && op[0] == '>' && op[1] == '=') return c >= 0;
    if (oi == 2 && op[0] == '<' && op[1] == '=') return c <= 0;
    if (op[0] == '>') return c > 0;
    if (op[0] == '<') return c < 0;
    return 0;
}

/* ver satisfies a comma-separated spec like ">=24,<25" */
static int spec_match(const char *ver, const char *spec) {
    char tmp[256];
    strncpy(tmp, spec, 255);
    tmp[255] = '\0';
    char *p = tmp;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != ',') p++;
        char hold = *p;
        *p = '\0';
        if (!cond_match(ver, start)) return 0;
        if (hold) *p = hold;
    }
    return 1;
}

/* Pick the HIGHEST variant matching current OS + version spec from a `variants`
 * array; returns the selected variant JsonValue* or NULL. */
static const char *cur_os_name(void) {
    const char *ov = getenv("PMM_OS_OVERRIDE"); /* debug/testing */
    if (ov && *ov) return ov;
    switch (pmm_detect_os()) { case OS_WINDOWS: return "windows"; case OS_LINUX: return "linux";
                               case OS_MACOS: return "macos"; default: return "any"; }
}
static const char *cur_arch_name(void) {
    const char *av = getenv("PMM_ARCH_OVERRIDE"); /* debug/testing */
    if (av && *av) return av;
    return pmm_detect_arch();
}

/* ---------- dependency resolution (Depends: "name (op ver), ...") ---------- */
static const char *dep_seen[128];
static int dep_seen_n = 0;

static int install_dep_spec(const char *depstr);   /* fwd (mutually recursive) */

static int parse_dep(const char *d, char *name, size_t ns, char *spec, size_t ss) {
    const char *paren = strchr(d, '(');
    if (paren) {
        size_t nl = (size_t)(paren - d);
        while (nl && (d[nl-1]==' '||d[nl-1]=='\t')) nl--;
        if (nl >= ns) nl = ns-1;
        memcpy(name, d, nl); name[nl] = 0;
        const char *c = paren + 1, *ce = strchr(c, ')');
        if (!ce) ce = c + strlen(c);
        size_t sl = (size_t)(ce - c); if (sl >= ss) sl = ss-1;
        memcpy(spec, c, sl); spec[sl] = 0;
    } else {
        size_t nl = strlen(d);
        while (nl && (d[nl-1]==' '||d[nl-1]=='\t')) nl--;
        if (nl >= ns) nl = ns-1;
        memcpy(name, d, nl); name[nl] = 0; spec[0] = 0;
    }
    char *p = name; while (*p==' '||*p=='\t') p++;
    if (p != name) memmove(name, p, strlen(p)+1);
    int se = (int)strlen(spec); while (se && (spec[se-1]==' '||spec[se-1]=='\t')) spec[--se]=0;
    return name[0] ? 0 : -1;
}

/* Install a comma-separated Depends list. Exposed so local .pdm installs resolve deps. */
int pmm_install_dep_list(const char *list) {
    if (!list || !*list) return 0;
    char *dup = strdup(list), *p = dup;
    while (p && *p) {
        char *end = strchr(p, ',');
        if (end) *end = 0;
        char *t = p; while (*t==' '||*t=='\t') t++;
        if (*t) install_dep_spec(t);
        if (!end) break;
        p = end + 1;
    }
    free(dup);
    return 0;
}

int install_from_registry(const char *name, const char *spec, const char *mirror_active) {
    if (!name || !*name) return -1;
    MirrorList *ml = mirrors_load();
    int has_reg = 0;
    for (int i = 0; i < ml->count; i++)
        if (ml->items[i].registry && *ml->items[i].registry) has_reg = 1;

    if (!has_reg) {
        pmm_error(
                "no registry mirror configured. Add to ~/.pmm/mirror.ini:\n"
                "     [name]\n     registry = https://host/pmm\n"
                "  (apt-style: mirrors are tried by priority; the first with the\n"
                "   package wins, else plain 'pmm install --git owner/repo')\n");
        mirrors_free(ml);
        return -1;
    }

    /* Build registry base order: active mirror first, then by priority, deduped. */
    char *bases[128];
    int nb = 0;
    int seen[128] = {0};
    if (mirror_active && *mirror_active) {
        for (int i = 0; i < ml->count; i++)
            if (ml->items[i].registry && *ml->items[i].registry &&
                strcmp(ml->items[i].name, mirror_active) == 0)
                { bases[nb++] = ml->items[i].registry; seen[i] = 1; break; }
    }
    for (int i = 0; i < ml->count && nb < 128; i++)
        if (!seen[i] && ml->items[i].registry && *ml->items[i].registry)
            bases[nb++] = ml->items[i].registry;

    if (nb == 0) { pmm_error(pmm_tr("msg.err.no-registry-mirror")); mirrors_free(ml); return -1; }

    /* fetch the <pkg>.json latest pointer (carries the variants list) */
    char url[2048];
    int status = 0;
    char *body = NULL;
    char *used_base = NULL;
    for (int i = 0; i < nb; i++) {
        char used = 0;
        /* layout migration: an old .../mirror/packages base also falls back to
         * .../mirror/dists, so stale mirror.ini keeps working after the reform. */
        char distsbase[2048];
        snprintf(distsbase, sizeof(distsbase), "%s", bases[i]);
        size_t dbl = strlen(distsbase);
        static const char *pkgsuf = "/packages";
        size_t dpl = strlen(pkgsuf);
        int isold = (dbl >= dpl && strcmp(distsbase + dbl - dpl, pkgsuf) == 0);
        if (isold) snprintf(distsbase + dbl - dpl, sizeof(distsbase) - (dbl - dpl), "/dists");

        for (int attempt = 0; attempt < (isold ? 2 : 1); attempt++) {
            const char *use = (attempt == 0) ? bases[i] : distsbase;
            snprintf(url, sizeof(url), "%s/%s.json", use, name);
            pmm_info("%s", pmm_tr_fmt("msg.looking-up", name, use));
            body = http_get(url, &status);
            if (getenv("PMM_DEBUG")) fprintf(stderr, "[reg] %s -> body=%s status=%d\n",
                                             use, body ? "set" : "NULL", status);
            if (body && status != 404 && status != 403 && status != 503) { used_base = bases[i]; used = 1; break; }
            free(body); body = NULL;
        }
        if (used) break;
    }
    if (!body) {
        pmm_error("%s", pmm_tr_fmt("msg.err.registry-not-found", name));
        mirrors_free(ml);
        return -1;
    }
    JsonValue *meta = json_parse(body);
    free(body);
    if (!meta || meta->type != JSON_OBJECT) {
        pmm_error("%s", pmm_tr_fmt("msg.err.bad-registry-entry", name, used_base));
        json_free(meta);
        mirrors_free(ml);
        return -1;
    }

    /* resolve declared dependencies before installing this package */
    JsonValue *deps = json_get(meta, "depends");
    if (deps && deps->type == JSON_ARRAY) {
        for (int i = 0; i < deps->count; i++) {
            JsonValue *dv = json_at(deps, i);
            if (dv && dv->type == JSON_STRING) install_dep_spec(dv->string);
        }
    }

    const char *osn = cur_os_name();
    const char *arch = cur_arch_name();
    JsonValue *variants = json_get(meta, "variants");
    const char *dl = NULL, *file = NULL;
    char *want_sha = NULL;
    const char *chosen = NULL;
    if (variants && variants->count > 0) {
        /* pick highest variant matching OS + ARCH (or 'any') + version spec */
        const char *bestver = NULL;
        for (int i = 0; i < variants->count; i++) {
            JsonValue *v = json_at(variants, i);
            if (!v || v->type != JSON_OBJECT) continue;
            const char *vos = json_str(v, "os"); if (!vos) continue;
            if (strcmp(vos, osn) != 0 && strcmp(vos, "any") != 0) continue;
            const char *varch = json_str(v, "arch");
            if (varch && varch[0] && strcmp(varch, arch) != 0 && strcmp(varch, "any") != 0) continue;
            const char *ver = json_str(v, "version"); if (!ver) continue;
            if (spec && *spec && !spec_match(ver, spec)) continue;
            if (!bestver || vcmp(ver, bestver) > 0) { bestver = ver; chosen = ver;
                dl = json_str(v, "url"); file = json_str(v, "file");
                const char *s = json_str(v, "sha256"); free(want_sha); want_sha = s ? strdup(s) : NULL; }
        }
        if (!chosen) {
            pmm_error("%s", pmm_tr_fmt("msg.no-version", name, osn, arch,
                    (spec && *spec)) ? " satisfying '" : "", (spec && *spec) ? spec : "");
            json_free(meta); mirrors_free(ml); return -1;
        }
        pmm_info("%s", pmm_tr_fmt("msg.selected", name, chosen, osn, arch));
    } else {
        /* legacy single-platform entry */
        dl = json_str(meta, "url"); file = json_str(meta, "file");
        const char *s = json_str(meta, "sha256"); want_sha = s ? strdup(s) : NULL;
        pmm_info("%s", pmm_tr_fmt("msg.selected", name, json_str(meta, "version"), osn, ""));
    }
    if (!dl) {
        pmm_error("%s", pmm_tr_fmt("msg.err.registry-entry-no-url", name));
        json_free(meta); mirrors_free(ml); return -1;
    }
    char *url_cp = strdup(dl);
    char *file_cp = file ? strdup(file) : strdup(base_name(url_cp));
    json_free(meta);
    mirrors_free(ml);

    int rc = install_file(url_cp, file_cp);

    if (want_sha && rc == 0) {
        char cache[1024], path[1200], hex[128];
        pmm_cache_dir(cache, sizeof(cache));
        snprintf(path, sizeof(path), "%s/%s", cache, file_cp);
        if (pmm_sha256_file(path, hex) == 0) {
            if (strcasecmp(want_sha, hex) != 0) {
                pmm_error("%s", pmm_tr_fmt("msg.checksum-mismatch", name, want_sha, hex));
                remove(path);
                rc = -1;
            } else {
                pmm_success("%s", pmm_tr_fmt("msg.registry-ok", hex));
            }
        }
    }
    free(url_cp); free(file_cp); free(want_sha);
    return rc;
}

/* ---------- installed-package DB queries (dependency solving) ---------- */

/* Extract the first value of `key` from a control/.info text buffer. Returns a
 * malloc'd copy or NULL. Multi-value fields (Depends/Provides/Conflicts) are
 * returned as their single comma-separated line. */
static char *ctl_field(const char *ctl, const char *key) {
    size_t klen = strlen(key);
    const char *line = ctl;
    while (line && *line) {
        const char *eol = strchr(line, '\n');
        size_t ll = eol ? (size_t)(eol - line) : strlen(line);
        if (ll > klen && strncasecmp(line, key, klen) == 0 && line[klen] == ':') {
            const char *v = line + klen + 1;
            while (v < line + ll && (*v == ' ' || *v == '\t')) v++;
            size_t vl = (size_t)(line + ll - v);
            while (vl && (v[vl-1] == ' ' || v[vl-1] == '\r') ) vl--;
            char *out = malloc(vl + 1);
            if (!out) return NULL;
            memcpy(out, v, vl); out[vl] = '\0';
            return out;
        }
        line = eol ? eol + 1 : NULL;
    }
    return NULL;
}

static char *read_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f); fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* Comma/space-separated Provides or Conflicts list contains `name`? */
static int field_has(const char *fieldval, const char *name) {
    if (!fieldval || !*fieldval) return 0;
    size_t nl = strlen(name);
    const char *p = fieldval;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        const char *s = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\t') p++;
        if ((size_t)(p - s) == nl && strncasecmp(s, name, nl) == 0) return 1;
    }
    return 0;
}

/* Does an installed package provide `name` (as its own Package, or via a
 * Provides: entry)? If found, store its installed Version into `ver_out`.
 * Returns 1 if `name` is satisfied by something installed, else 0. */
static int installed_provides(const char *name, char *ver_out, size_t vs) {
    char home[1024];
    pmm_config_dir(home, sizeof(home));
    char db[1200];
    snprintf(db, sizeof(db), "%s/installed", home);
    (void)ver_out; (void)vs;
#ifndef _WIN32
    DIR *d = opendir(db);
    if (!d) return 0;
    int hit = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t ln = strlen(e->d_name);
        if (ln < 5 || strcmp(e->d_name + ln - 5, ".info") != 0) continue;
        char path[1400];
        snprintf(path, sizeof(path), "%s/%s", db, e->d_name);
        char *info = read_text_file(path);
        if (!info) continue;
        char *pkg = ctl_field(info, "Package");
        char *ver = ctl_field(info, "Version");
        char *prov = ctl_field(info, "Provides");
        if (pkg && strcmp(pkg, name) == 0) {
            if (ver_out && ver) snprintf(ver_out, vs, "%s", ver);
            hit = 1;
        } else if (field_has(prov, name)) {
            if (ver_out && ver) snprintf(ver_out, vs, "%s", ver);
            hit = 1;
        }
        free(pkg); free(ver); free(prov); free(info);
        if (hit) break;
    }
    closedir(d);
    return hit;
#else
    return 0;   /* conservative: never claim a dep is satisfied on Windows */
#endif
}

/* Is the requested dep (name + optional version spec) already satisfied by an
 * installed package (own name or a Provides), matching the version range?
 * Returns 1 = satisfied (skip install), 0 = must install. */
static int installed_satisfies(const char *depName, const char *spec) {
    char ver[128] = "";
    if (!installed_provides(depName, ver, sizeof(ver)) || !ver[0]) return 0;
    if (!spec || !*spec) return 1;
    return spec_match(ver, spec) ? 1 : 0;
}

/* Does `conflicts` (a comma list) name anything already installed? Used to
 * reject an install that would clash with the current environment. */
int any_installed_conflict(const char *conflicts) {
    if (!conflicts || !*conflicts) return 0;
    char *dup = strdup(conflicts), *p = dup;
    int hit = 0;
    while (p && *p) {
        char *end = strchr(p, ',');
        if (end) *end = '\0';
        char *t = p; while (*t == ' ' || *t == '\t') t++;
        if (*t && installed_provides(t, NULL, 0)) { hit = 1; break; }
        if (!end) break;
        p = end + 1;
    }
    free(dup);
    return hit;
}

static int install_dep_spec(const char *depstr) {
    char name[256], spec[256];
    if (parse_dep(depstr, name, sizeof(name), spec, sizeof(spec)) != 0) return -1;
    /* If this dep is already satisfied by an installed package (its own
     * name or a Provides), don't re-install it. */
    if (installed_satisfies(name, spec)) {
        pmm_info("%s", pmm_tr_fmt("msg.dep-satisfied", name, spec[0] ? " " : "", spec));
        return 0;
    }
    for (int i = 0; i < dep_seen_n; i++)
        if (strcmp(dep_seen[i], name) == 0) return 0;   /* cycle / duplicate */
    if (dep_seen_n < 128) dep_seen[dep_seen_n++] = strdup(name);
    pmm_info("%s", pmm_tr_fmt("msg.resolving-dep", name, spec[0] ? " " : "", spec));
    return install_from_registry(name, spec[0] ? spec : NULL, NULL);
}
