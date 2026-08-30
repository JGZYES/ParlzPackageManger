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
            printf("pmm: warning: could not parse %s checksum file\n", algos[i].ext);
            continue;
        }
        /* case-insensitive compare */
        if (strcasecmp(expect, hex) != 0) {
            fprintf(stderr, "pmm: CHECKSUM MISMATCH (%s)!\n  expected: %s\n  actual:   %s\n",
                    algos[i].ext, expect, hex);
            return -1;
        }
        printf("pmm: %s checksum OK: %s\n", algos[i].algo == 256 ? "sha256" : "sha1", hex);
    }
    return 0;
}

int install_file(const char *url, const char *name) {
    PmmOS os = pmm_detect_os();
    char cache[1024], path[1200], cmd[2600];
    pmm_cache_dir(cache, sizeof(cache));
    snprintf(path, sizeof(path), "%s/%s", cache, name);

    /* apt-style fallback: mirrors (by priority) first, then the origin URL */
    MirrorList *ml = mirrors_load();
    int ncand = 0;
    char **cands = mirrors_download_candidates(ml, url, &ncand);
    int ok = -1;
    for (int i = 0; i < ncand; i++) {
        printf("pmm: downloading %s%s\n", cands[i],
               i < ncand - 1 ? " (mirror)" : "");
        if (http_download(cands[i], path) == 0) { ok = 0; break; }
        fprintf(stderr, "pmm: download failed, trying next source...\n");
        remove(path);
    }
    for (int i = 0; i < ncand; i++) free(cands[i]);
    free(cands);
    mirrors_free(ml);
    if (ok != 0) {
        fprintf(stderr, "pmm: all download sources failed: %s\n", url);
        return -1;
    }
    printf("pmm: downloaded %s\n", path);

    /* integrity: sha256 + sha1, verified against sidecar checksums when present */
    char hex[128];
    if (pmm_sha256_file(path, hex) == 0)
        printf("pmm: sha256: %s\n", hex);
    if (pmm_sha1_file(path, hex) == 0)
        printf("pmm: sha1:   %s\n", hex);
    if (verify_checksums(url, path, name) != 0) {
        fprintf(stderr, "pmm: refusing to install: checksum verification failed\n");
        remove(path);
        return -1;
    }

    const char *bname = base_name(name);
    char dest[1024];
    pmm_install_dir(dest, sizeof(dest));

    /* forward-slash clone of the cached path (native exes/tools accept it and the
     * git-bash shell won't mangle a command word like "C:/...") */
    char pwin[1200];
    snprintf(pwin, sizeof(pwin), "%s", path);
    for (char *q = pwin; *q; q++) if (*q == '\\') *q = '/';

    /* .pdm packages are installed through the deb-like manager */
    if (has_suffix(bname, ".pdm") || has_suffix(bname, ".PDM")) {
        extern int pdm_install_file(const char *path);
        printf("pmm: installing .pdm %s ...\n", bname);
        if (pdm_install_file(path) != 0) {
            fprintf(stderr, "pmm: .pdm install failed: %s\n", bname);
            return -1;
        }
        printf("pmm: installed .pdm %s\n", bname);
        pmm_add_to_path();
        return 0;
    }

    if (has_suffix(bname, ".deb") && os == OS_LINUX) {
        snprintf(cmd, sizeof(cmd), "sudo dpkg -i \"%s\"", path);
    } else if (has_suffix(bname, ".rpm") && os == OS_LINUX) {
        snprintf(cmd, sizeof(cmd), "sudo rpm -Uvh \"%s\"", path);
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
        /* unknown type for this OS: just leave in cache and copy if a single file */
        snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s/\" 2>/dev/null || true", path, dest);
    }

    printf("pmm: installing %s ...\n", bname);
    if (run_cmd_quiet(cmd) != 0) {
        fprintf(stderr, "pmm: install command failed: %s\n", cmd);
        return -1;
    }
    printf("pmm: installed %s\n", bname);
    pmm_add_to_path();
    return 0;
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

int install_from_registry(const char *name, const char *spec, const char *mirror_active) {
    if (!name || !*name) return -1;
    MirrorList *ml = mirrors_load();
    int has_reg = 0;
    for (int i = 0; i < ml->count; i++)
        if (ml->items[i].registry && *ml->items[i].registry) has_reg = 1;

    if (!has_reg) {
        fprintf(stderr,
                "pmm: no registry mirror configured. Add to ~/.pmm/mirror.ini:\n"
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

    if (nb == 0) { fprintf(stderr, "pmm: no registry mirrors\n"); mirrors_free(ml); return -1; }

    /* fetch the <pkg>.json latest pointer (carries the variants list) */
    char url[2048];
    int status = 0;
    char *body = NULL;
    char *used_base = NULL;
    for (int i = 0; i < nb; i++) {
        snprintf(url, sizeof(url), "%s/%s.json", bases[i], name);
        printf("pmm: looking up %s in mirror %s\n", name, bases[i]);
        body = http_get(url, &status);
        if (body && status == 200) { used_base = bases[i]; break; }
        free(body); body = NULL;
    }
    if (!body) {
        fprintf(stderr, "pmm: package '%s' not found in any registry mirror\n", name);
        mirrors_free(ml);
        return -1;
    }
    JsonValue *meta = json_parse(body);
    free(body);
    if (!meta || meta->type != JSON_OBJECT) {
        fprintf(stderr, "pmm: bad registry entry for '%s' (%s)\n", name, used_base);
        json_free(meta);
        mirrors_free(ml);
        return -1;
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
            fprintf(stderr, "pmm: no version of '%s' for os=%s arch=%s%s%s\n", name, osn, arch,
                    (spec && *spec) ? " satisfying '" : "", (spec && *spec) ? spec : "");
            json_free(meta); mirrors_free(ml); return -1;
        }
        printf("pmm: selected %s@%s (os=%s arch=%s)\n", name, chosen, osn, arch);
    } else {
        /* legacy single-platform entry */
        dl = json_str(meta, "url"); file = json_str(meta, "file");
        const char *s = json_str(meta, "sha256"); want_sha = s ? strdup(s) : NULL;
        printf("pmm: selected %s@%s (os=%s)\n", name, json_str(meta, "version"), osn);
    }
    if (!dl) {
        fprintf(stderr, "pmm: registry entry for '%s' has no url\n", name);
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
                fprintf(stderr, "pmm: CHECKSUM MISMATCH (%s registry entry)!\n"
                                "  expected: %s\n  actual:   %s\n", name, want_sha, hex);
                remove(path);
                rc = -1;
            } else {
                printf("pmm: registry sha256 OK: %s\n", hex);
            }
        }
    }
    free(url_cp); free(file_cp); free(want_sha);
    return rc;
}
