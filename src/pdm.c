/* pdm.c - .pdm pack/install/remove (deb-like, built on system tar) */
#include "pdm.h"
#include "pmm.h"
#include "out.h"
#include "i18n.h"
#include "install.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <inttypes.h>
#define PMM_MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#define PMM_POPEN_READ(cmd) _popen(cmd, "r")
#define PMM_PCLOSE_READ(p) _pclose(p)
#define GETCWD(p,n) (_getcwd((p),(n)))
#define CHDIR(p) _chdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#define PMM_MKDIR(p) mkdir((p), 0755)
#define RMDIR(p) rmdir(p)
#define PMM_POPEN_READ(cmd) popen(cmd, "r")
#define PMM_PCLOSE_READ(p) pclose(p)
#define GETCWD(p,n) getcwd((p),(n))
#define CHDIR(p) chdir(p)
#endif

/* ---------- small helpers ---------- */

static int ends_with(const char *s, const char *suf) {
    size_t sl = strlen(s), xl = strlen(suf);
    return sl >= xl && strcmp(s + sl - xl, suf) == 0;
}

static const char *base_name(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *p = (a > b) ? a : b;
    return p ? p + 1 : path;
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p; *p = '\0'; PMM_MKDIR(tmp); *p = c;
        }
    }
    PMM_MKDIR(tmp);
}

/* Convert backslashes to forward slashes (so cmd/bash don't treat "C:\x" as host). */
static void rmtree(const char *dir) {
    /* Pure-C recursive delete: no shell, so no path-flavour (bsdtar/GNU) issues. */
#ifdef _WIN32
    char patt[1200];
    snprintf(patt, sizeof(patt), "%s\\*", dir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(patt, &fd);
    if (h != -1) {
        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            char full[1400];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.name);
            if (fd.attrib & _A_SUBDIR) rmtree(full);
            else remove(full);
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
    RMDIR(dir);
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[1400];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) rmtree(full);
        else remove(full);
    }
    closedir(d);
    RMDIR(dir);
#endif
}

/* KEY: value lookup in control text; returns malloc'd value or NULL. */
static char *control_get(const char *text, const char *key) {
    size_t klen = strlen(key);
    const char *line = text;
    while (line && *line) {
        const char *eol = strchr(line, '\n');
        size_t ll = eol ? (size_t)(eol - line) : strlen(line);
        if (ll > klen && strncasecmp(line, key, klen) == 0 && line[klen] == ':') {
            const char *v = line + klen + 1;
            while (v < line + ll && (*v == ' ' || *v == '\t')) v++;
            size_t vl = (size_t)(line + ll - v);
            while (vl && (v[vl-1] == '\r' || v[vl-1] == ' ')) vl--;
            char *out = malloc(vl + 1);
            memcpy(out, v, vl);
            out[vl] = '\0';
            return out;
        }
        line = eol ? eol + 1 : NULL;
    }
    return NULL;
}

static char *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    if (len) *len = (long)rd;
    return buf;
}

/* ---------- pack ---------- */

int pdm_pack(const char *dir, const char *out) {
    char cp_[1100];
    snprintf(cp_, sizeof(cp_), "%s/pdm-control", dir);
    char *control = read_file(cp_, NULL);
    if (!control) {
        pmm_error("%s", pmm_tr_fmt("msg.err.no-control", dir));
        return -1;
    }
    char *pkg = control_get(control, "Package");
    char *ver = control_get(control, "Version");
    if (!pkg || !*pkg) {
        pmm_error(pmm_tr("msg.err.no-package"));
        free(control); return -1;
    }
    if (!ver || !*ver) ver = strdup("0.0.0");

    /* Output defaults to <Package>_<Version>.pdm in the current directory.
     * Everything below uses paths relative to the CWD so the shell never sees a
     * Windows drive-letter path (which MSYS bash would mangle). */
    char outbuf[1100];
    if (!out) {
        snprintf(outbuf, sizeof(outbuf), "%s_%s.pdm", pkg, ver);
        out = outbuf;
    }

    const char *stage = ".pdm-stage";
    char cmd[2800];
    rmtree(stage);
    PMM_MKDIR(stage);

    /* control.tar.gz (contains pdm-control; packed from the source dir) */
    snprintf(cmd, sizeof(cmd), "tar -czf \"%s/control.tar.gz\" -C \"%s\" pdm-control", stage, dir);
    if (system(cmd) != 0) { pmm_error(pmm_tr("msg.err.tar-control")); rmtree(stage); return -1; }

    /* data.tar.gz (everything except pdm-control and the scratch dir) */
    snprintf(cmd, sizeof(cmd),
             "tar -czf \"%s/data.tar.gz\" -C \"%s\" --exclude pdm-control --exclude %s .",
             stage, dir, stage);
    if (system(cmd) != 0) { pmm_error(pmm_tr("msg.err.tar-data")); rmtree(stage); return -1; }

    /* sha256sums of the two members */
    char sums_path[1100], hex[128];
    snprintf(sums_path, sizeof(sums_path), "%s/sha256sums", stage);
    FILE *sf = fopen(sums_path, "w");
    if (sf) {
        char mp[1200];
        snprintf(mp, sizeof(mp), "%s/control.tar.gz", stage);
        if (pmm_sha256_file(mp, hex) == 0) fprintf(sf, "%s  control.tar.gz\n", hex);
        snprintf(mp, sizeof(mp), "%s/data.tar.gz", stage);
        if (pmm_sha256_file(mp, hex) == 0) fprintf(sf, "%s  data.tar.gz\n", hex);
        fclose(sf);
    }

    /* outer archive */
    snprintf(cmd, sizeof(cmd), "tar -cf \"%s\" -C \"%s\" control.tar.gz data.tar.gz sha256sums", out, stage);
    int rc = system(cmd);
    rmtree(stage);
    if (rc != 0) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-write", out)); return -1; }

    pmm_success("%s", pmm_tr_fmt("msg.packed", dir, pkg, ver, out));
    free(control); free(pkg);
    if (strcmp(ver, "0.0.0") != 0) free(ver);
    return 0;
}

/* ---------- install ---------- */

static void db_dirs(char *db, size_t dbs, char *root, size_t roots) {
    char home[900];
    pmm_config_dir(home, sizeof(home) - 32);
    snprintf(db, dbs, "%s/installed", home);
    mkdir_p(db);
    if (pmm_flat_mode()) snprintf(root, roots, "%s", home);      /* -p <path> */
    else snprintf(root, roots, "%s/root", home);
    mkdir_p(root);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return -1; }
    fclose(in);
    fclose(out);
    return 0;
}

/* Recursively find the first .exe under dir (for the app icon); malloc'd path or NULL. */
static char *find_first_exe(const char *dir) {
#ifdef _WIN32
    char patt[1300];
    snprintf(patt, sizeof(patt), "%s\\*", dir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(patt, &fd);
    if (h != -1) {
        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            char full[1400];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.name);
            if ((fd.attrib & _A_SUBDIR)) {
                char *r = find_first_exe(full);
                if (r) { _findclose(h); return r; }
            } else {
                size_t l = strlen(fd.name);
                if (l > 4 && strcasecmp(fd.name + l - 4, ".exe") == 0) {
                    char *r = malloc(1400);
                    if (r) snprintf(r, 1400, "%s\\%s", dir, fd.name);
                    _findclose(h);
                    return r;
                }
            }
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char full[1400];
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            struct stat st;
            if (stat(full, &st) == 0) {
                if (S_ISDIR(st.st_mode)) { char *r = find_first_exe(full); if (r) { closedir(d); return r; } }
            }
        }
        closedir(d);
    }
#endif
    return NULL;
}

/* Recursively sum file sizes under dir, in bytes. */
static unsigned long long dir_size(const char *dir) {
    unsigned long long total = 0;
#ifdef _WIN32
    char patt[1300];
    snprintf(patt, sizeof(patt), "%s\\*", dir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(patt, &fd);
    if (h != -1) {
        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            char full[1400];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.name);
            if (fd.attrib & _A_SUBDIR) total += dir_size(full);
            else total += (unsigned long long)fd.size;
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char full[1400];
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            struct stat st;
            if (stat(full, &st) == 0) {
                if (S_ISDIR(st.st_mode)) total += dir_size(full);
                else total += (unsigned long long)st.st_size;
            }
        }
        closedir(d);
    }
#endif
    return total;
}

static char saved_cwd[1400];
static int chdir_save(const char *dir) {
    if (!GETCWD(saved_cwd, sizeof(saved_cwd))) saved_cwd[0] = '\0';
    return CHDIR(dir) == 0 ? 0 : -1;
}
static void chdir_restore(void) {
    if (saved_cwd[0]) CHDIR(saved_cwd);
}

/* Move everything under <dir>/bin up into <dir>, then remove bin/. (C-level, so
 * it works on Windows cmd too where `mv` doesn't exist.) */
static void flatten_bin(const char *dir) {
    char bindir[1200];
    snprintf(bindir, sizeof(bindir), "%s/%s", dir, "bin");
#ifdef _WIN32
    char patt[1300];
    snprintf(patt, sizeof(patt), "%s\\*", bindir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(patt, &fd);
    if (h != -1) {
        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            char src[1400], dst[1400];
            snprintf(src, sizeof(src), "%s\\%s", bindir, fd.name);
            snprintf(dst, sizeof(dst), "%s\\%s", dir, fd.name);
            rename(src, dst);
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
    RMDIR(bindir);
#else
    DIR *d = opendir(bindir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char src[1400], dst[1400];
            snprintf(src, sizeof(src), "%s/%s", bindir, e->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", dir, e->d_name);
            rename(src, dst);
        }
        closedir(d);
        rmdir(bindir);
    }
#endif
}

/* Case-insensitive path equality (Windows paths/FAT are case-insensitive, and
 * separators may be '/' or '\'); the running exe path uses backslashes while the
 * install target uses forward slashes. */
static int path_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca == '/' || ca == '\\') ca = '/';
        if (cb == '/' || cb == '\\') cb = '/';
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* If `target` is the currently running pmm executable, move it aside to
 * `<target>.old` so a .pdm reinstall can write the new binary (a running image
 * can be renamed, but not overwritten). Returns 1 if moved, 0 otherwise. */
static int move_self_aside(const char *target) {
    const char *self = pmm_self_path();
    if (!self || !*self) return 0;
    if (!path_eq_ci(target, self)) return 0;
    char old[1500];
    snprintf(old, sizeof(old), "%s.old", target);
    remove(old);                        /* Windows rename can't clobber */
    if (rename(target, old) != 0) return 0;
    return 1;
}

/* Restore a previously-aside `<target>.old` back to `target` (best-effort). */
static void restore_aside(const char *target) {
    char old[1500];
    snprintf(old, sizeof(old), "%s.old", target);
    remove(target);
    rename(old, target);
}

int pdm_info(const char *pdmfile) {
    pmm_info("%s\n", base_name(pdmfile));
    /* Copy into the pm dir + chdir, then use relative names (works with both
     * MSYS GNU tar and System32 bsdtar). */
    char home[1024];
    pmm_config_dir(home, sizeof(home));
    char info_tmp[1400];
    snprintf(info_tmp, sizeof(info_tmp), "%s/_pmm_info.pdm", home);
    if (copy_file(pdmfile, info_tmp) != 0) return -1;
    if (chdir_save(home) != 0) { remove(info_tmp); return -1; }
    char cmd[2800];
    snprintf(cmd, sizeof(cmd), "tar -xOf _pmm_info.pdm control.tar.gz | tar -xzOf - pdm-control");
    int rc = system(cmd);
    chdir_restore();
    remove(info_tmp);
    return rc == 0 ? 0 : -1;
}

int pdm_install_file(const char *pdmfile) {
    if (!ends_with(pdmfile, ".pdm")) {
        pmm_error("%s", pmm_tr_fmt("msg.err.not-pdm", pdmfile));
        return -1;
    }
    char home[1024], db[1024], root[1024], cmd[2600];
    pmm_config_dir(home, sizeof(home));
    int flat = pmm_flat_mode();             /* -p <path>: deploy directly into that dir */
    snprintf(db, sizeof(db), "%s/installed", home);
    mkdir_p(db);
    if (flat)
        snprintf(root, sizeof(root), "%s", home);
    else
        snprintf(root, sizeof(root), "%s/root", home);
    mkdir_p(root);

    /* Copy the package into the PM dir, chdir there, and run tar with ONLY
     * relative paths. Relative paths are understood identically by MSYS GNU tar
     * and System32 bsdtar, so we don't have to guess a drive-letter style. */
    char tmpname[1400], tmprel[] = "_pmm_install.pdm";
    snprintf(tmpname, sizeof(tmpname), "%s/_pmm_install.pdm", home);
    if (copy_file(pdmfile, tmpname) != 0) {
        pmm_error("%s", pmm_tr_fmt("msg.err.cannot-read", pdmfile));
        return -1;
    }
    if (chdir_save(home) != 0) { remove(tmpname); return -1; }
    if (system(NULL) == 0) { pmm_error(pmm_tr("msg.err.no-tar")); chdir_restore(); remove(tmpname); return -1; }

    const char *stage = "installed/.stage";
    rmtree(stage);
    PMM_MKDIR(stage);

    /* extract members (all relative) */
    snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", tmprel, stage);
    if (system(cmd) != 0) {
        pmm_error("%s", pmm_tr_fmt("msg.err.bad-archive", pdmfile));
        chdir_restore(); remove(tmpname); return -1;
    }

    /* verify member checksums (relative to pm dir) */
    char hex[128];
    FILE *sf = fopen("installed/.stage/sha256sums", "r");
    if (sf) {
        char line[256], fname[128], expect[128];
        while (fgets(line, sizeof(line), sf)) {
            if (sscanf(line, "%127s  %127s", expect, fname) != 2) continue;
            char mp[1200];
            snprintf(mp, sizeof(mp), "installed/.stage/%s", fname);
            if (pmm_sha256_file(mp, hex) != 0 || strcasecmp(hex, expect) != 0) {
                pmm_error("%s", pmm_tr_fmt("msg.err.checksum-mismatch-file", fname, pdmfile));
                fclose(sf); chdir_restore(); remove(tmpname); return -1;
            }
        }
        fclose(sf);
        pmm_success(pmm_tr("msg.checksum-ok"));
    } else {
        pmm_warn(pmm_tr("msg.warn.no-checksum"));
    }

    /* read control */
    snprintf(cmd, sizeof(cmd), "tar -xzOf \"%s/control.tar.gz\" pdm-control", stage);
    FILE *cf = PMM_POPEN_READ(cmd);
    if (!cf) {
        pmm_error("%s", pmm_tr_fmt("msg.err.no-control-tar", pdmfile));
        chdir_restore(); remove(tmpname); return -1;
    }
    char ctl[4096];
    size_t got = fread(ctl, 1, sizeof(ctl) - 1, cf);
    PMM_PCLOSE_READ(cf);
    ctl[got] = '\0';
    /* resolve declared dependencies from the registry before installing */
    char *deps = control_get(ctl, "Depends");
    if (deps && *deps) { pmm_install_dep_list(deps); free(deps); }
    char *pkg = control_get(ctl, "Package");
    char *ver = control_get(ctl, "Version");
    if (!pkg || !*pkg) {
        pmm_error(pmm_tr("msg.err.no-package"));
        chdir_restore(); remove(tmpname); return -1;
    }

    /* extract data into root (relative target: cwd is already the pm dir) */
    const char *tgt = flat ? "." : "root";
    /* If the package is pmm itself, its exe is the currently running binary and
     * Windows/locked images can't be overwritten by tar. Move it aside first so
     * the new binary can be written; restore on failure. */
    char self_target[1500];
    snprintf(self_target, sizeof(self_target), "%s/bin/pmm.exe", root);
    int self_moved = move_self_aside(self_target);
    snprintf(cmd, sizeof(cmd), "tar -xzf \"%s/data.tar.gz\" -C \"%s\"", stage, tgt);
    if (system(cmd) != 0) {
        pmm_error(pmm_tr("msg.err.extract"));
        if (self_moved) restore_aside(self_target);   /* put old binary back */
        chdir_restore(); remove(tmpname); return -1;
    }
    if (self_moved) {
        char old[1600];
        snprintf(old, sizeof(old), "%s.old", self_target);
        remove(old);                                  /* drop the old image */
    }

    /* flat mode (-p <path>): move bin/* up into the address itself, drop bin/ */
    if (flat) {
        flatten_bin(".");   /* cwd = the install address in flat mode */
    }

    /* record file list + control in db */
    snprintf(cmd, sizeof(cmd), "%s/%s.info", db, pkg);
    FILE *dbf = fopen(cmd, "wb"); /* binary: keep \n, avoid Windows CRLF translation */
    if (dbf) {
        fprintf(dbf, "Package: %s\nVersion: %s\nSource: %s\n\n%s\n",
                pkg, ver ? ver : "0.0.0", base_name(pdmfile), ctl);
        fprintf(dbf, "Files:\n");
        char ltcmd[2600];
        snprintf(ltcmd, sizeof(ltcmd), "tar -tzf \"%s/data.tar.gz\"", stage);
        FILE *tf = PMM_POPEN_READ(ltcmd);
        if (tf) {
            char ln[2048];
            while (fgets(ln, sizeof(ln), tf)) {
                if (flat) { /* reflection of the flatten: strip a leading bin/ */
                    const char *q = ln;
                    while (*q==' '||*q=='\t') q++;
                    if (q[0]=='.'&&q[1]=='/'&&strncasecmp(q+2,"bin/",4)==0) { ln[0]='\0'; continue; }
                }
                fputs(ln, dbf);
            }
            PMM_PCLOSE_READ(tf);
        } else {
            pmm_warn(pmm_tr("msg.warn.no-filelist"));
        }
        fclose(dbf);
    }

    rmtree(stage);
    chdir_restore();
    remove(tmpname);
    pmm_success("installed %s %s into %s\n", pkg, ver ? ver : "0.0.0", root);

    /* register in the Windows per-user "Installed Apps" (Uninstall) hive,
     * like an MSI installer would, so it shows in Settings > Installed apps */
    {
        char rootpkg[1400];
        snprintf(rootpkg, sizeof(rootpkg), "%s/%s", root, pkg);
        char *icon = find_first_exe(rootpkg);
        const char *pub = control_get(ctl, "Maintainer");
        const char *self = pmm_self_path();
        pmm_reg_uninstall(pkg, ver ? ver : "", pub ? pub : "",
                          root, icon, self && *self ? self : "pmm",
                          dir_size(rootpkg));
        if (pub) free((char *)pub);
        free(icon);
    }

    pmm_add_to_path();  /* auto-put PMM dirs (e.g. root/nodejs) on PATH */

#ifndef _WIN32
    /* On Linux/macOS, mark anything in <root>/bin executable. Windows doesn't
     * record unix exec bits, so ELF/scripts packaged on Windows lose +x; restore
     * them at install time so e.g. a Linux Node.js binary actually runs. */
    {
        char cx[1600];
        snprintf(cx, sizeof(cx), "chmod +x \"%s/bin\"/* 2>/dev/null || true", root);
        system(cx);
    }
#endif

    free(pkg); if (ver) free(ver);
    return 0;
}

/* ---------- list / remove ---------- */

void pdm_list_installed(void) {
    char db[1024], root[1024];
    db_dirs(db, sizeof(db), root, sizeof(root));
#ifdef _WIN32
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\\*.info\" 2>nul", db);
#else
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "ls -1 \"%s\"/*.info 2>/dev/null", db);
#endif
    printf("installed .pdm packages (root: %s):\n", root);
    fflush(stdout);
    system(cmd);
}

int pdm_remove(const char *name) {
    char db[1024], root[1024];
    db_dirs(db, sizeof(db), root, sizeof(root));
    char ipath[1100];
    snprintf(ipath, sizeof(ipath), "%s/%s.info", db, name);
    long len = 0;
    char *info = read_file(ipath, &len);
    if (!info) {
        pmm_error("%s", pmm_tr_fmt("msg.err.not-installed", name));
        return -1;
    }
    char *files = strstr(info, "Files:\n");
    int n = 0;
    if (files) {
        char path[1024];
        char *line = files + 7;
        while (*line) {
            char *eol = strchr(line, '\n');
            if (!eol) eol = line + strlen(line);
            size_t ll = (size_t)(eol - line);
            while (ll && (line[ll-1] == '\r' || line[ll-1] == ' ')) ll--;
            if (ll > 0 && ll < sizeof(path)) {
                memcpy(path, line, ll); path[ll] = '\0';
                /* normalize "./x" -> "x" */
                char *p = path;
                while (p[0] == '.' && p[1] == '/') p += 2;
                char full[1200];
                snprintf(full, sizeof(full), "%s/%s", root, p);
                remove(full);
                n++;
            }
            line = *eol ? eol + 1 : eol;
        }
    }
    remove(ipath);
    free(info);
    pmm_success("%s", pmm_tr_fmt("msg.removed", name, n));
    pmm_reg_uninstall_clear(name);  /* drop it from Installed Apps */
    return 0;
}
