/* main.c - ParlzPackageManger (PMM)
 *
 * Usage:
 *   pmm install <pkg>                 install from configured registry
 *   pmm install --git <owner/repo>    install latest release asset for this OS
 *   pmm install --git <url>           full URL: github/gitlab/gitea/forgejo
 *   pmm install --host <host> --git <slug>   force host (github|gitlab|gitea|forgejo)
 *   pmm mirror list                   list configured mirrors
 *   pmm mirror add <name> <api-url>   add a mirror (writes mirror.ini)
 *   pmm mirror remove <name>          remove a mirror
 *   pmm mirror use <name>             set active mirror
 *   pmm list                          list installed packages
 *   pmm version | help
 *
 * Config: ~/.pmm/pmm.json | pmm.ini | pmm.conf (first found wins)
 * Mirrors: ~/.pmm/mirror.ini | mirror.conf
 */
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pmm.h"
#include "out.h"
#include "i18n.h"
#include "json.h"
#include "ini.h"
#include "http.h"
#include "repo.h"
#include "install.h"
#include "pdm.h"
#include "mirrors.h"
#include "sha256.h"
#include "sha1.h"

#if defined(_WIN32)
#include <direct.h>
#define PMM_MKDIR(p) _mkdir(p)
#else
#define PMM_MKDIR(p) mkdir((p), 0755)
#endif

typedef struct {
    char *registry_url;   /* package registry base URL */
    char *mirror_name;    /* active mirror name ("" = none) */
    char *language;       /* active locale / language pack ("" = default) */
} PmmConfig;

typedef struct {
    char *name;           /* active mirror name or NULL */
    char *api_base;       /* mirror API base, NULL = use default host URLs */
} MirrorSel;

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* ---------- config loading (json / ini / conf) ---------- */

static void load_config(PmmConfig *cfg) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    memset(cfg, 0, sizeof(*cfg));
    char *path = pmm_find_config(dir, "pmm");
    if (!path) return;
    if (strstr(path, ".json")) {
        JsonValue *root = json_parse_file(path);
        if (root) {
            const char *u = json_str(root, "registry");
            if (u) cfg->registry_url = xstrdup(u);
            const char *m = json_str(root, "mirror");
            if (m) cfg->mirror_name = xstrdup(m);
            const char *l = json_str(root, "language");
            if (l) cfg->language = xstrdup(l);
            json_free(root);
        }
    } else {
        Ini *ini = ini_load(path);
        if (ini) {
            const char *u = ini_get(ini, NULL, "registry");
            if (!u) u = ini_get(ini, "pmm", "registry");
            if (u) cfg->registry_url = xstrdup(u);
            const char *m = ini_get(ini, NULL, "mirror");
            if (!m) m = ini_get(ini, "pmm", "mirror");
            if (m) cfg->mirror_name = xstrdup(m);
            const char *l = ini_get(ini, NULL, "language");
            if (!l) l = ini_get(ini, "pmm", "language");
            if (l) cfg->language = xstrdup(l);
            ini_free(ini);
        }
    }
}

static const char *mirror_section_name(void) {
#if defined(_WIN32)
    return pmm_os_name(pmm_detect_os()); /* per-OS section falls back below */
#else
    return pmm_os_name(pmm_detect_os());
#endif
}

/* Load mirrors from mirror.ini / mirror.conf; pick active mirror (by cfg
 * name, first "default=true", or first entry). Format:
 *   [name]
 *   api = https://mirror.example.com   (rewrites github api; or gitea api base)
 *   default = true|false
 */
static void load_mirror(PmmConfig *cfg, MirrorSel *sel) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    pmm_ensure_default_mirror(dir);
    memset(sel, 0, sizeof(*sel));
    char *path = pmm_find_config(dir, "mirror");
    if (!path) return;
    Ini *ini = ini_load(path);
    if (!ini) return;

    /* discover section names (order of appearance) */
    const char *first = NULL, *chosen = NULL;
    for (IniEntry *e = ini->head; e; e = e->next) {
        if (!*e->section) continue;
        if (!first) first = e->section;
        const char *dflt = ini_get(ini, e->section, "default");
        if (dflt && (strcmp(dflt, "true") == 0 || strcmp(dflt, "1") == 0) && !chosen)
            chosen = e->section;
    }
    const char *want = (cfg->mirror_name && *cfg->mirror_name) ? cfg->mirror_name : chosen;
    if (!want) want = first;
    if (!want) { ini_free(ini); return; }

    sel->name = xstrdup(want);
    const char *api = ini_get(ini, want, "api");
    if (!api) api = ini_get(ini, mirror_section_name(), "api");
    sel->api_base = xstrdup(api);
    ini_free(ini);
}

/* ---------- commands ---------- */

static const char *host_name(PmmHost h) {
    switch (h) {
    case HOST_GITHUB:  return "github";
    case HOST_GITLAB:  return "gitlab";
    case HOST_GITEA:   return "gitea";
    case HOST_FORGEJO: return "forgejo";
    default:           return "auto";
    }
}

static PmmHost host_from_name(const char *h) {
    if (strcmp(h, "github") == 0) return HOST_GITHUB;
    if (strcmp(h, "gitlab") == 0) return HOST_GITLAB;
    if (strcmp(h, "gitea") == 0) return HOST_GITEA;
    if (strcmp(h, "forgejo") == 0) return HOST_FORGEJO;
    return HOST_UNKNOWN;
}

/* True if the filename has a known auto-installable extension (local packages). */
static int has_installer_ext(const char *s) {
    static const char *exts[] = { ".deb", ".rpm", ".appimage", ".msi", ".exe", ".zip",
                                  ".tgz", ".tar.gz", ".tar.xz", ".tar.bz2", ".7z",
                                  ".dmg", ".pkg", ".pkg.tar.zst", NULL };
    size_t l = strlen(s);
    for (int i = 0; exts[i]; i++) {
        size_t el = strlen(exts[i]);
        if (l >= el && strcmp(s + l - el, exts[i]) == 0) return 1;
    }
    return 0;
}

/* Case-insensitive substring (avoids non-standard strcasestr). */
static int ci_contains(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (!nl) return 1;
    for (; *hay; hay++) {
        size_t i;
        for (i = 0; i < nl && hay[i]; i++)
            if (tolower((unsigned char)hay[i]) != tolower((unsigned char)needle[i])) break;
        if (i == nl) return 1;
    }
    return 0;
}

/* Fetch `{registry}/{rel}` from the configured mirrors (by priority).
 * Returns malloc'd body or NULL; *outstatus receives the HTTP code. */
static char *registry_fetch(const char *rel, int *outstatus) {
    MirrorList *ml = mirrors_load();
    char *bases[128]; int nb = 0;
    for (int i = 0; i < ml->count && nb < 128; i++)
        if (ml->items[i].registry && *ml->items[i].registry)
            bases[nb++] = ml->items[i].registry;
    char url[2048]; int status = 0; char *body = NULL;
    for (int i = 0; i < nb; i++) {
        snprintf(url, sizeof(url), "%s/%s", bases[i], rel);
        body = http_get(url, &status);
        if (body && status != 404 && status != 403 && status != 503) break;
        free(body); body = NULL;
    }
    mirrors_free(ml);
    if (outstatus) *outstatus = status;
    return body;
}

/* pmm search <keyword> — list registry packages matching keyword. */
static int cmd_search(int argc, char **argv) {
    if (argc < 1) { pmm_error("%s", pmm_tr("msg.err.usage")); return 1; }
    int status = 0;
    char *body = registry_fetch("packages.json", &status);
    if (!body) { pmm_error("%s", pmm_tr("msg.err.no-registry-index")); return 1; }
    JsonValue *root = json_parse(body);
    free(body);
    if (!root || root->type != JSON_ARRAY) { pmm_error("%s", pmm_tr("msg.err.bad-registry-index")); json_free(root); return 1; }
    const char *kw = argv[0];
    int found = 0;
    for (int i = 0; i < root->count; i++) {
        JsonValue *v = json_at(root, i);
        const char *name = (v && v->type == JSON_STRING) ? v->string : NULL;
        if (!name) continue;
        if (ci_contains(name, kw)) { printf("  %s\n", name); found++; }
    }
    json_free(root);
    if (!found) pmm_info("%s", pmm_tr_fmt("msg.no-match", kw));
    return 0;
}

/* pmm info <package|file.pdm> — registry package info, or a local .pdm's control. */
static int cmd_info(int argc, char **argv) {
    if (argc < 1) { pmm_error("usage: pmm info <package|file.pdm>\n"); return 1; }
    const char *pkg = argv[0];
    /* local .pdm -> control info */
    FILE *chk = fopen(pkg, "rb");
    if (chk) {
        fclose(chk);
        size_t lp = strlen(pkg);
        if (lp > 4 && (strcmp(pkg + lp - 4, ".pdm") == 0 || strcmp(pkg + lp - 4, ".PDM") == 0))
            return pdm_info(pkg);
    }
    /* registry package */
    char rel[512]; snprintf(rel, sizeof(rel), "%s.json", pkg);
    int status = 0;
    char *body = registry_fetch(rel, &status);
    if (!body) { pmm_error("%s", pmm_tr_fmt("msg.err.not-found", pkg)); return 1; }
    JsonValue *root = json_parse(body);
    free(body);
    if (!root || root->type != JSON_OBJECT) { pmm_error("%s", pmm_tr_fmt("msg.err.bad-entry", pkg)); json_free(root); return 1; }
    const char *name = json_str(root, "name");
    const char *ver  = json_str(root, "version");
    pmm_info("%s%s%s\n", name ? name : pkg, ver ? " " : "", ver ? ver : "");
    const char *desc = json_str(root, "description");
    if (desc) pmm_info("%s", pmm_tr_fmt("msg.description", desc));
    JsonValue *vr = json_get(root, "variants");
    if (vr && vr->count > 0) {
        printf("  versions:\n");
        for (int i = 0; i < vr->count; i++) {
            JsonValue *v = json_at(vr, i);
            if (!v) continue;
            const char *vv = json_str(v, "version");
            const char *osn = json_str(v, "os");
            const char *archn = json_str(v, "arch");
            printf("    %-12s %s/%s\n", vv ? vv : "?", osn ? osn : "-", archn ? archn : "-");
        }
    } else {
        const char *u = json_str(root, "url");
        const char *s256 = json_str(root, "sha256");
        printf("  url: %s\n", u ? u : "-");
        if (s256) printf("  sha256: %s\n", s256);
    }
    json_free(root);
    return 0;
}

/* List the *.info files in `dir` into a caller array `names[]` (up to `cap`
 * entries). Returns the count. */
static int list_info_files(const char *dir, char names[][1400], int cap) {
    int n = 0;
#ifdef _WIN32
    char pat[1500]; snprintf(pat, sizeof(pat), "%s\\*.info", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (n < cap) snprintf(names[n++], 1400, "%s/%s", dir, fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t ln = strlen(e->d_name);
        if (ln < 5 || strcmp(e->d_name + ln - 5, ".info") != 0) continue;
        if (n < cap) snprintf(names[n++], 1400, "%s/%s", dir, e->d_name);
    }
    closedir(d);
#endif
    return n;
}

/* Compare two dotted versions ("1.0.51" vs "1.0.5"), numeric component-wise.
 * Returns >0 if a is newer, <0 if b newer, 0 if equal. Mirrors install.c vcmp. */
static int cmp_version(const char *a, const char *b) {
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

/* Read "Package:" and "Version:" from the first lines of an installed .info.
 * The header block is authoritative (the embedded ctl repeats them lower). */
static void installed_version(const char *info, char *pkg, size_t pkgsz, char *ver, size_t versz) {
    const char *p = info;
    pkg[0] = ver[0] = '\0';
    while (p && *p) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);
        char line[512];
        if (ll >= sizeof(line)) ll = sizeof(line) - 1;
        memcpy(line, p, ll); line[ll] = '\0';
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            const char *key = line, *val = colon + 1;
            while (*val == ' ' || *val == '\t') val++;
            char *e = val + strlen(val);
            while (e > val && (e[-1] == ' ' || e[-1] == '\r')) *--e = '\0';
            if (strcmp(key, "Package") == 0) snprintf(pkg, pkgsz, "%s", val);
            else if (strcmp(key, "Version") == 0) snprintf(ver, versz, "%s", val);
        }
        if (!eol) break;
        p = eol + 1;
    }
}

/* pmm upgrade — for each installed .pdm, look up the registry latest version
 * and offer to upgrade. With --yes, upgrade without prompting. */
static int cmd_upgrade(int argc, char **argv) {
    int yes = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--yes") == 0) { yes = 1; continue; }
        /* a short flag cluster (-y, -qy, -yq): any 'y' means yes */
        if (a[0] == '-' && a[1] && a[1] != '-') {
            for (const char *p = a + 1; *p; p++) if (*p == 'y') { yes = 1; break; }
        }
    }

    char home[1024];
    pmm_config_dir(home, sizeof(home));
    char db[1200];
    snprintf(db, sizeof(db), "%s/installed", home);

    PmmConfig cfg; MirrorSel mirror;
    load_config(&cfg); load_mirror(&cfg, &mirror);

    char files[128][1400];
    int nfiles = list_info_files(db, files, 128);
    if (nfiles == 0) { pmm_info("%s", pmm_tr("msg.no-installed")); return 0; }

    int upgraded = 0, checked = 0;
    for (int fi = 0; fi < nfiles; fi++) {
        const char *ipath = files[fi];
        char info[4096];
        FILE *fp = fopen(ipath, "rb");
        if (!fp) continue;
        size_t got = fread(info, 1, sizeof(info) - 1, fp);
        fclose(fp);
        info[got] = '\0';

        char pkg[256], curver[128];
        installed_version(info, pkg, sizeof(pkg), curver, sizeof(curver));
        if (!pkg[0]) continue;
        checked++;

        /* fetch latest for this package (current os/arch) */
        char rel[512]; snprintf(rel, sizeof(rel), "%s.json", pkg);
        int status = 0;
        char *body = registry_fetch(rel, &status);
        if (!body) { pmm_warn("%s", pmm_tr_fmt("msg.skipping-no-registry", pkg)); continue; }
        JsonValue *root = json_parse(body);
        free(body);
        if (!root) continue;

        const char *osn = pmm_os_name(pmm_detect_os());
        const char *arn = pmm_detect_arch();
        const char *best = NULL;
        JsonValue *vr = json_get(root, "variants");
        if (vr && vr->count > 0) {
            for (int i = 0; i < vr->count; i++) {
                JsonValue *v = json_at(vr, i);
                if (!v || v->type != JSON_OBJECT) continue;
                const char *vos = json_str(v, "os");
                if (!vos || (strcmp(vos, osn) != 0 && strcmp(vos, "any") != 0)) continue;
                const char *va = json_str(v, "arch");
                if (va && va[0] && strcmp(va, arn) != 0 && strcmp(va, "any") != 0) continue;
                const char *vv = json_str(v, "version");
                if (!vv) continue;
                if (!best || cmp_version(vv, best) > 0) best = vv;
            }
        } else {
            best = json_str(root, "version");
        }
        /* Copy best into a stable buffer BEFORE freeing the JSON tree: best
         * currently points into json_str()'s internal storage. */
        char bestbuf[128];
        if (best) snprintf(bestbuf, sizeof(bestbuf), "%s", best);
        json_free(root);
        if (!best) continue;

        if (cmp_version(bestbuf, curver) <= 0) { pmm_info("%s", pmm_tr_fmt("msg.up-to-date", pkg, curver, bestbuf)); continue; }

        pmm_info("%s: %s -> %s\n", pkg, curver, bestbuf);
        if (!yes) {
            printf("  upgrade? [y/N] ");
            fflush(stdout);
            char ans[8] = "";
            if (!fgets(ans, sizeof(ans), stdin)) ans[0] = '\0';
            if (!(ans[0] == 'y' || ans[0] == 'Y')) continue;
        }
        if (install_from_registry(pkg, NULL, mirror.name) == 0) { upgraded++; }
        else pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", pkg));
    }

    free(cfg.registry_url); free(cfg.mirror_name);
    free(mirror.name); free(mirror.api_base);
    if (checked == 0) pmm_info("%s", pmm_tr("msg.no-installed"));
    else pmm_success("%s", pmm_tr_fmt("msg.upgraded", upgraded));
    return 0;
}

/* Write `body` to ~/.pmm/cache/registry/<rel>. Returns 0 on success. */
static int save_registry_cache(const char *rel, const char *body) {
    char cache[1024];
    pmm_cache_dir(cache, sizeof(cache));
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/registry", cache);
    /* mkdir -p via a small loop */
    char tmp[1200]; snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) if (*p == '/') { char c = *p; *p = '\0'; PMM_MKDIR(tmp); *p = c; }
    PMM_MKDIR(dir);
    char path[1400];
    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(body, 1, strlen(body), f);
    fclose(f);
    return 0;
}

/* pmm update — apt-style: refresh the local registry index. Downloads
 * packages.json plus every {pkg}.json from the configured mirrors into
 * ~/.pmm/cache/registry/ so search/info/upgrade can consult them. */
static int cmd_update(void) {
    int status = 0;
    char *body = registry_fetch("packages.json", &status);
    if (!body) { pmm_error("no registry index (packages.json) available\n"); return 1; }
    JsonValue *root = json_parse(body);
    if (!root || root->type != JSON_ARRAY) {
        pmm_error("bad registry index\n"); if (root) json_free(root); free(body); return 1;
    }
    int total = root->count, ok = 0;
    if (total == 0) { pmm_info("%s", pmm_tr("msg.registry-empty")); json_free(root); free(body); return 0; }
    save_registry_cache("packages.json", body);
    json_free(root);

    for (int i = 0; i < total; i++) {
        /* re-parse to get each name */
        JsonValue *r2 = json_parse(body);
        if (!r2) break;
        JsonValue *v = json_at(r2, i);
        const char *name = (v && v->type == JSON_STRING) ? v->string : NULL;
        if (!name) { json_free(r2); continue; }
        char rel[512]; snprintf(rel, sizeof(rel), "%s.json", name);
        int st = 0;
        char *pkgbody = registry_fetch(rel, &st);
        if (pkgbody && st != 404 && st != 403 && st != 503) {
            if (save_registry_cache(rel, pkgbody) == 0) ok++;
        }
        free(pkgbody);
        json_free(r2);
    }
    free(body);
    pmm_success("%s", pmm_tr_fmt("msg.registry-updated", ok, total));
    return 0;
}

/* pmm verify <file> — print sha256 (and sha1) of a downloaded package file. */
static int cmd_verify(int argc, char **argv) {
    if (argc < 1) { pmm_error("usage: pmm verify <file>\n"); return 1; }
    const char *file = argv[0];
    char hex[128];
    if (pmm_sha256_file(file, hex) == 0) pmm_success("%s", pmm_tr_fmt("msg.sha256", hex, file));
    else { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-read", file)); return 1; }
    if (pmm_sha1_file(file, hex) == 0) pmm_success("%s", pmm_tr_fmt("msg.sha1", hex, file));
    return 0;
}

/* Recursively delete a directory (for the download cache). */
static void rm_tree(const char *dir) {
#ifdef _WIN32
    char pat[1200]; snprintf(pat, sizeof(pat), "%s\\*", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            char full[1400]; snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rm_tree(full);
            else DeleteFileA(full);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char full[1400]; snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) rm_tree(full);
        else remove(full);
    }
    closedir(d);
    rmdir(dir);
#endif
}

/* pmm cache clean — wipe the download cache. */
static int cmd_cache_clean(void) {
    char dir[1024];
    pmm_cache_dir(dir, sizeof(dir));
    pmm_info("%s", pmm_tr_fmt("msg.cleaning-cache", dir));
    rm_tree(dir);
    return 0;
}

/* <flag> <repo-ish> [--host <name>] */
static int cmd_install_git(int argc, char **argv, const char *flag) {
    const char *repo = NULL;
    const char *branch = NULL;
    PmmHost host = HOST_AUTO;
    PmmConfig cfg; MirrorSel mirror;
    load_config(&cfg);
    load_mirror(&cfg, &mirror);

    if (strcmp(flag, "github") == 0) host = HOST_GITHUB;
    else if (strcmp(flag, "gitlab") == 0) host = HOST_GITLAB;
    else if (strcmp(flag, "gitea") == 0) host = HOST_GITEA;
    else if (strcmp(flag, "forgejo") == 0) host = HOST_FORGEJO;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = host_from_name(argv[++i]);
            if (host == HOST_UNKNOWN) {
                pmm_error("%s", pmm_tr_fmt("msg.err.unknown-host", argv[i]));
                return 1;
            }
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--branch") == 0) && i + 1 < argc) {
            branch = argv[++i];
        } else if (!repo) {
            repo = argv[i];
        }
    }
    if (!repo) {
        pmm_error(
            "usage:\n"
            "  pmm install --git <repo-url.git>     any git host (auto-detected API)\n"
            "  pmm install --github <owner/repo>    GitHub\n"
            "  pmm install --gitlab <owner/repo>    GitLab\n"
            "  pmm install --gitea <url/owner/repo> Gitea / Forgejo (incl. self-hosted)\n"
            "  [--host github|gitlab|gitea|forgejo] force host type\n");
        return 1;
    }
    if (host == HOST_AUTO) host = repo_detect_host(repo);

    RepoContext *ctx = repo_open(host, repo, mirror.api_base);
    if (!ctx) return 1;
    if (branch && *branch) { free(ctx->ref); ctx->ref = strdup(branch); }
    pmm_info("host=%s repo=%s%s%s\n", host_name(host), repo,
           mirror.name ? " mirror=" : "", mirror.name ? mirror.name : "");

    PmmOS os = pmm_detect_os();
    ReleaseAsset *asset = repo_latest_asset(ctx, os);
    if (!asset && host == HOST_AUTO)
        pmm_warn("could not reach a known release API for %s "
                "(tried gitea/gitlab/github shapes)\n", repo);
    if (!asset) {
        if (host != HOST_AUTO)
            pmm_error("%s", pmm_tr_fmt("msg.err.no-suitable-asset", pmm_os_name(os), repo));
        repo_close(ctx);
        return 1;
    }
    pmm_info("%s -> %s (tag %s)\n", pmm_os_name(os), asset->name, asset->tag);
    int rc = install_file(asset->url, asset->name);

    /* fallback: the first pick may have been a cross-platform/wrong asset
     * (e.g. fzf's Android binary named without an OS hint). Re-fetch a clearly
     * os-named one and install that instead. */
    if (rc != 0 && host != HOST_AUTO) {
        const char *sub = (os == OS_LINUX) ? "linux" : (os == OS_MACOS) ? "darwin" : NULL;
        if (sub) {
            ReleaseAsset *fb = repo_asset_matching(ctx, os, sub);
            if (fb && strcmp(fb->name, asset->name) != 0) {
                pmm_warn("%s", pmm_tr_fmt("msg.retrying-asset", fb->name, pmm_os_name(os)));
                rc = install_file(fb->url, fb->name);
                repo_asset_free(fb);
            }
        }
    }
    repo_asset_free(asset);
    repo_close(ctx);
    return rc == 0 ? 0 : 1;
}

static int cmd_mirror(int argc, char **argv) {
    char dir[1024], path[1200];
    pmm_config_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/mirror.ini", dir);

    if (argc == 0 || strcmp(argv[0], "list") == 0) {
        Ini *ini = ini_load(path);
        if (!ini || !ini->head) { printf("(no mirrors configured)\n"); ini_free(ini); return 0; }
        printf("mirrors in %s:\n", path);
        /* one line per unique section */
        char lastsec[512] = "";
        for (IniEntry *e = ini->head; e; e = e->next) {
            if (!*e->section) continue;
            if (strcmp(e->section, lastsec) == 0) continue;
            strncpy(lastsec, e->section, sizeof(lastsec) - 1);
            const char *api = ini_get(ini, e->section, "api");
            const char *reg = ini_get(ini, e->section, "registry");
            const char *dl = ini_get(ini, e->section, "download");
            const char *dflt = ini_get(ini, e->section, "default");
            printf("  [%s] registry=%s api=%s download=%s%s\n", e->section,
                   reg ? reg : "-", api ? api : "-", dl ? dl : "-",
                   dflt && strcmp(dflt, "true") == 0 ? " (default)" : "");
        }
        ini_free(ini);
        return 0;
    }
    /* pmm mirror check — probe each configured registry mirror's reachability
     * (apt-style source check). Reports priority + ok/fail for each source. */
    if (strcmp(argv[0], "check") == 0) {
        Ini *ini = ini_load(path);
        if (!ini || !ini->head) { pmm_error("%s", pmm_tr("msg.err.no-mirror")); ini_free(ini); return 1; }
        printf("checking registry mirrors...\n");
        char lastsec[512] = "";
        int reachable = 0, total = 0;
        for (IniEntry *e = ini->head; e; e = e->next) {
            if (!*e->section) continue;
            if (strcmp(e->section, lastsec) == 0) continue;
            strncpy(lastsec, e->section, sizeof(lastsec) - 1);
            const char *reg = ini_get(ini, e->section, "registry");
            const char *pri = ini_get(ini, e->section, "priority");
            const char *dflt = ini_get(ini, e->section, "default");
            if (!reg || !*reg) continue;   /* source without a registry isn't probeable */
            total++;
            char url[2048];
            snprintf(url, sizeof(url), "%s/packages.json", reg);
            /* probe with a short GET; capture status code */
            int st = 0;
            char *body = http_get(url, &st);
            int ok = (body && st != 404 && st != 403 && st != 503 && st != 0);
            free(body);
            const char *stext = ok ? "ok"    :
                                (st == 404) ? "not found" :
                                (st == 0)   ? "unreachable" : "HTTP %d";
            printf("  [%s] pri=%s %s%s   %s\n", e->section,
                   pri ? pri : "-", reg, dflt && strcmp(dflt,"true")==0 ? " (default)" : "",
                   ok ? "OK" : stext);
            if (ok) reachable++;
        }
        ini_free(ini);
        printf("result: %d/%d sources reachable\n", reachable, total);
        return reachable > 0 ? 0 : 1;
    }
    if (strcmp(argv[0], "add") == 0 && argc >= 3) {
        FILE *f = fopen(path, "a");
        if (!f) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-write", path)); return 1; }
        fprintf(f, "\n[%s]\napi = %s\n", argv[1], argv[2]);
        fclose(f);
        pmm_success("%s", pmm_tr_fmt("msg.mirror-added", argv[1]));
        return 0;
    }
    if (strcmp(argv[0], "use") == 0 && argc >= 2) {
        /* set mirror=<name> in pmm.conf (always writable text format) */
        char cfgpath[1200];
        snprintf(cfgpath, sizeof(cfgpath), "%s/pmm.conf", dir);
        FILE *rf = fopen(cfgpath, "r");
        char lines[256][1024]; int n = 0;
        if (rf) {
            char line[1024];
            while (fgets(line, sizeof(line), rf) && n < 256) {
                if (strncmp(line, "mirror", 6) != 0) strncpy(lines[n++], line, sizeof(lines[0]) - 1);
            }
            fclose(rf);
        }
        FILE *wf = fopen(cfgpath, "w");
        if (!wf) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-write", cfgpath)); return 1; }
        int wrote = 0;
        for (int i = 0; i < n; i++) fputs(lines[i], wf);
        fprintf(wf, "mirror = %s\n", argv[1]);
        (void)wrote;
        fclose(wf);
        pmm_success("%s", pmm_tr_fmt("msg.mirror-active", argv[1]));
        return 0;
    }
    if (strcmp(argv[0], "remove") == 0 && argc >= 2) {
        Ini *ini = ini_load(path);
        if (!ini) { pmm_error("%s", pmm_tr("msg.err.no-mirror-file")); return 1; }
        FILE *f = fopen(path, "w");
        if (!f) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-write", path)); ini_free(ini); return 1; }
        char cursec[512] = "";
        for (IniEntry *e = ini->head; e; e = e->next) {
            if (strcmp(e->section, cursec) != 0) {
                strncpy(cursec, e->section, sizeof(cursec) - 1);
                if (strcmp(e->section, argv[1]) != 0)
                    fprintf(f, "\n[%s]\n", e->section);
            }
            if (strcmp(e->section, argv[1]) != 0)
                fprintf(f, "%s = %s\n", e->key, e->value);
        }
        fclose(f);
        ini_free(ini);
        pmm_success("%s", pmm_tr_fmt("msg.mirror-removed", argv[1]));
        return 0;
    }
    pmm_error("%s", pmm_tr("msg.err.usage"));
    return 1;
}

static void print_help(void) {
    printf("PMM %s (%s)\n\n", PMM_VERSION, pmm_detect_arch());
    printf("%s\n", pmm_tr("help.usage"));
    printf("  %-32s%s\n", "pmm install <pkg>        ", pmm_tr("desc.install"));
    printf("  %-32s%s\n", "pmm search <keyword>     ", pmm_tr("desc.search"));
    printf("  %-32s%s\n", "pmm info <pkg|file.pdm>  ", pmm_tr("desc.info"));
    printf("  %-32s%s\n", "pmm remove <pkg>         ", pmm_tr("desc.remove"));
    printf("  %-32s%s\n", "pmm update               ", pmm_tr("desc.update"));
    printf("  %-32s%s\n", "pmm upgrade [--yes]      ", pmm_tr("desc.upgrade"));
    printf("  %-32s%s\n", "pmm mirror ...           ", pmm_tr("desc.mirror"));
    printf("  %-32s%s\n", "pmm self-update          ", pmm_tr("desc.self-update"));
    printf("  %-32s%s\n", "pmm version | help       ", pmm_tr("desc.help"));
    printf("\n%s\n", pmm_tr("help.options"));
    printf("  %-32s%s\n", "-p<drive>  ", pmm_tr("opt.p-drive"));
    printf("  %-32s%s\n", "--no-color ", pmm_tr("opt.no-color"));
    printf("  %-32s%s\n", "-q, --quiet", pmm_tr("opt.quiet"));
    printf("  %-32s%s\n", "--verbose  ", pmm_tr("opt.verbose"));
    printf("\nconfig: <base>/pmm.json | pmm.ini | pmm.conf  (base = <drive>:\\\\.pmm 或 ~/.pmm)\n");
    printf("mirrors: <base>/mirror.ini | mirror.conf\n");
    printf("asset mapping: windows=exe/msi/zip/7z  linux=deb/rpm/appimage/tar.*  macos=dmg/pkg\n");
}

/* Make every directory component of `path` using PMM_MKDIR (mkdir -p). */
static void mkdir_p_local(char *path) {
    for (char *p = path + 1; *p; p++)
        if (*p == '/' || *p == '\\') { char c = *p; *p = '\0'; PMM_MKDIR(path); *p = c; }
    PMM_MKDIR(path);
}

/* pmm setting lang ... */
static int cmd_setting(int argc, char **argv) {
    if (argc < 1 || strcmp(argv[0], "lang") != 0) {
        pmm_error("%s", pmm_tr("msg.err.usage"));
        return 1;
    }
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    char langdir[1200];
    snprintf(langdir, sizeof(langdir), "%s/lang", dir);

    /* -l : list installed language packs */
    if (argc == 2 && strcmp(argv[1], "-l") == 0) {
        printf("language packs in %s:\n", langdir);
#ifdef _WIN32
        char pat[1300]; snprintf(pat, sizeof(pat), "%s\\*.pjson", langdir);
        WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do { printf("  %s\n", fd.cFileName); } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
#else
        DIR *dd = opendir(langdir);
        if (dd) {
            struct dirent *ee;
            while ((ee = readdir(dd)) != NULL) {
                size_t ln = strlen(ee->d_name);
                if (ln > 6 && strcmp(ee->d_name + ln - 6, ".pjson") == 0) printf("  %s\n", ee->d_name);
            }
            closedir(dd);
        }
#endif
        return 0;
    }

    /* install / activate a specific locale: requires a value */
    if (argc < 2) { pmm_error("%s", pmm_tr("msg.err.usage")); return 1; }
    const char *loc = argv[1];
    if (strchr(loc, '/') || strchr(loc, '\\') || strstr(loc, "..")) { pmm_error("%s", pmm_tr_fmt("msg.err.invalid-locale", loc)); return 1; }

    /* pick a registry base for the language pack */
    MirrorList *ml = mirrors_load();
    const char *base = NULL;
    for (int i = 0; i < ml->count; i++)
        if (ml->items[i].registry && *ml->items[i].registry) { base = ml->items[i].registry; break; }
    if (!base) { pmm_error("%s", pmm_tr("msg.err.no-registry-mirror")); mirrors_free(ml); return 1; }

    char url[2048];
    snprintf(url, sizeof(url), "%s/lang/%s.pjson", base, loc);
    mkdir_p_local(langdir);
    char out[1400];
    snprintf(out, sizeof(out), "%s/%s.pjson", langdir, loc);
    if (http_download(url, out) != 0) {
        pmm_error("%s", pmm_tr_fmt("msg.err.download-failed", loc));
        mirrors_free(ml); return 1;
    }
    mirrors_free(ml);

    /* persist language=<loc> in pmm.conf (always-writable text) */
    char cfgpath[1200];
    snprintf(cfgpath, sizeof(cfgpath), "%s/pmm.conf", dir);
    char lines[256][1024]; int nn = 0;
    FILE *rf = fopen(cfgpath, "r");
    if (rf) {
        char line[1024];
        while (fgets(line, sizeof(line), rf) && nn < 256) {
            if (strncmp(line, "language", 8) != 0) strncpy(lines[nn++], line, sizeof(lines[0]) - 1);
        }
        fclose(rf);
    }
    FILE *wf = fopen(cfgpath, "w");
    if (!wf) { pmm_error("%s", pmm_tr_fmt("msg.err.cannot-write", cfgpath)); return 1; }
    for (int i = 0; i < nn; i++) fputs(lines[i], wf);
    fprintf(wf, "language = %s\n", loc);
    fclose(wf);

    pmm_lang_set_locale(loc);
    pmm_lang_load(out);
    pmm_success("%s", pmm_tr_fmt("msg.lang-set", loc));
    return 0;
}

/* Parse "-p<drive>" / "-p <drive>" flags (e.g. -pd -> D:\.pmm) anywhere in
 * argv, record the drive, and compact the argument list so subcommands don't
 * see the flag. Returns new argc. */
static int consume_drive_flag(int argc, char **argv) {
    int w = 1; /* argv[0] is the program name */
    for (int r = 1; r < argc; r++) {
        const char *a = argv[r];
        if (a[0] == '-' && a[1] == 'p' && (a[2] >= 'A' && a[2] <= 'Z' || a[2] >= 'a' && a[2] <= 'z')) {
            char dr[2] = { a[2], '\0' };
            pmm_set_install_drive(dr);
            continue;
        }
        if (strcmp(a, "-p") == 0 && r + 1 < argc) {
            const char *nxt = argv[r + 1];
            if (nxt[0] && (nxt[0] >= 'A' && nxt[0] <= 'Z' || nxt[0] >= 'a' && nxt[0] <= 'z') && nxt[1] == '\0') {
                char dr[2] = { nxt[0], '\0' };
                pmm_set_install_drive(dr);
                r++; /* consume the drive arg too */
                continue;
            }
            /* -p <exact path>: install under this full address */
            pmm_set_install_path(nxt);
            r++;
            continue;
        }
        argv[w++] = argv[r];
    }
    return w;
}

/* Strip global output flags (--no-color/-q/--quiet/--verbose) from argv before
 * dispatch, so every subcommand gets the same behaviour without each having to
 * parse them. Also honours PMM_NO_COLOR=1 env. Returns the new argc. */
static int consume_global_flags(int argc, char **argv) {
    int w = 1;
    for (int r = 1; r < argc; r++) {
        const char *a = argv[r];
        /* A combined short flag cluster (e.g. -yq / -qy): consume the global
         * chars (q=quiet, v=verbose); keep any non-global chars (e.g. y=yes)
         * as a fresh "-y" token for the subcommand. */
        if (a[0] == '-' && a[1] && a[1] != '-' && a[2]) {
            char keep[64]; int ki = 0;
            for (const char *p = a + 1; *p && ki < 62; p++) {
                if (*p == 'q') { pmm_log_level = 1; continue; }
                if (*p == 'v') { pmm_log_level = 2; continue; }
                keep[ki++] = *p;
            }
            if (ki > 0) {
                char buf[64]; buf[0] = '-'; memcpy(buf + 1, keep, ki); buf[ki + 1] = '\0';
                argv[w++] = strdup(buf);
            }
            continue;
        }
        if (strcmp(a, "--no-color") == 0) { pmm_no_color = 1; continue; }
        if (strcmp(a, "-q") == 0 || strcmp(a, "--quiet") == 0) { pmm_log_level = 1; continue; }
        if (strcmp(a, "--verbose") == 0) { pmm_log_level = 2; continue; }
        argv[w++] = argv[r];
    }
    if (getenv("PMM_NO_COLOR")) pmm_no_color = 1;
    return w;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   /* UTF-8 output so Chinese help/version isn't mojibake */
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Enable ANSI/VT colour on modern Windows consoles (legacy gets it too via
     * ENABLE_VIRTUAL_TERMINAL_PROCESSING). Without this, "\033[31m" shows as
     * literal text. Do it for both stdout and stderr. */
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h && h != INVALID_HANDLE_VALUE) {
            DWORD m = 0;
            if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        h = GetStdHandle(STD_ERROR_HANDLE);
        if (h && h != INVALID_HANDLE_VALUE) {
            DWORD m = 0;
            if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
    if (argc < 2) { print_help(); return 0; }
    pmm_set_self_path(argv[0]);
    argc = consume_drive_flag(argc, argv);
    argc = consume_global_flags(argc, argv);
    if (argc < 2) { print_help(); return 0; }

    /* initialise i18n: use configured language (if any), then load its pack */
    {
        PmmConfig cfg; load_config(&cfg);
        if (cfg.language && *cfg.language) pmm_lang_set_locale(cfg.language);
        free(cfg.registry_url); free(cfg.mirror_name); free(cfg.language);
        pmm_lang_load_active();
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help(); return 0;
    }
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        pmm_info("PMM %s (%s)\n", PMM_VERSION, pmm_detect_arch());
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) { pdm_list_installed(); return 0; }
    if (strcmp(argv[1], "mirror") == 0) return cmd_mirror(argc - 2, argv + 2);
    if (strcmp(argv[1], "setting") == 0) return cmd_setting(argc - 2, argv + 2);
    /* pmm remove <pkg>  (also used by the Windows "Uninstall" registration) */
    if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) { pmm_error("%s", pmm_tr("msg.err.usage")); return 1; }
        return pdm_remove(argv[2]) == 0 ? 0 : 1;
    }
    /* pmm self-update: install the latest 'pmm' tool package (auto os+arch) */
    if (strcmp(argv[1], "self-update") == 0) {
        PmmConfig cfg; MirrorSel mirror;
        load_config(&cfg);
        load_mirror(&cfg, &mirror);
        pmm_info("self-update -> installing latest pmm (%s/%s)\n",
               pmm_os_name(pmm_detect_os()), pmm_detect_arch());
        int rc = install_from_registry("pmm", NULL, mirror.name);
        free(cfg.registry_url); free(cfg.mirror_name);
        free(mirror.name); free(mirror.api_base);
        if (rc == 0)
            pmm_success("updated. New tools are under %s/root/bin (pmm/pdm). Re-run from there, or add it to PATH.\n",
                   pmm_config_dir((char[1024]){0}, 1024));
        return rc == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "update") == 0)
        return cmd_update();

    if (strcmp(argv[1], "upgrade") == 0)
        return cmd_upgrade(argc - 2, argv + 2);

    if (strcmp(argv[1], "install") == 0) {
        if (argc >= 3 && (strcmp(argv[2], "--git") == 0 || strcmp(argv[2], "--github") == 0 ||
                          strcmp(argv[2], "--gitlab") == 0 || strcmp(argv[2], "--gitea") == 0 ||
                          strcmp(argv[2], "--forgejo") == 0))
            return cmd_install_git(argc - 3, argv + 3, argv[2] + 2);
    if (argc < 3) {
        pmm_error("usage: pmm install <pkg|file.deb|file.msi|file.rpm|file.pdm> [pkg2 ...]\n"
                        "           | pmm install -dpkg <x.deb>   install a .deb (Linux)\n"
                        "           | pmm install -rpm <x.rpm>    install an .rpm (any Linux, incl. Debian/RPM)\n"
                        "           | pmm install --git <repo>\n");
        return 1;
    }
    /* collect one or more package arguments (each may carry an inline version
     * spec like nodejs>=24,<25); optional -v/--version applies to a single one */
    const char *version = NULL;
    int forced = 0;   /* 1 = -dpkg, 2 = -msi, 3 = -rpm (install a local package by type) */
    const char *items[64]; int ni = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--no-cache") == 0) { pmm_no_cache = 1; continue; }
        if (strcmp(a, "-dpkg") == 0) { forced = 1; if (i + 1 < argc) items[ni++] = argv[++i]; continue; }
        if (strcmp(a, "-msi") == 0)  { forced = 2; if (i + 1 < argc) items[ni++] = argv[++i]; continue; }
        if (strcmp(a, "-rpm") == 0)  { forced = 3; if (i + 1 < argc) items[ni++] = argv[++i]; continue; }
        if (strncmp(a, "-v", 2) == 0 && a[2]) { version = a + 2; continue; }
        if (strcmp(a, "-v") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
        if (strcmp(a, "--version") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
        if (strncmp(a, "--version=", 10) == 0) { version = a + 10; continue; }
        if (a[0] != '-' && ni < 64) items[ni++] = a;
    }
    if (ni == 0) items[ni++] = argv[2];

    PmmConfig cfg; MirrorSel mirror;
    load_config(&cfg);
    load_mirror(&cfg, &mirror);
    int ok = 0;
    for (int k = 0; k < ni; k++) {
        const char *raw = items[k];
        char pkg[256], spec[256];
        if (ni == 1 && version && *version) {
            strncpy(pkg, raw, sizeof(pkg) - 1); pkg[sizeof(pkg)-1] = '\0';
            strncpy(spec, version, sizeof(spec) - 1); spec[sizeof(spec)-1] = '\0';
        } else {
            const char *ops[] = { ">=", "<=", "==", "!=", ">", "<", NULL };
            const char *pos = NULL;
            for (int i = 0; ops[i]; i++) { pos = strstr(raw, ops[i]); if (pos) break; }
            if (pos) {
                size_t n = (size_t)(pos - raw);
                if (n >= sizeof(pkg)) n = sizeof(pkg) - 1;
                memcpy(pkg, raw, n); pkg[n] = '\0';
                strncpy(spec, pos, sizeof(spec) - 1); spec[sizeof(spec)-1] = '\0';
            } else {
                strncpy(pkg, raw, sizeof(pkg) - 1); pkg[sizeof(pkg)-1] = '\0';
                spec[0] = '\0';
            }
        }
        size_t la = strlen(pkg);
        if (la > 4 && (strcmp(pkg + la - 4, ".pdm") == 0 || strcmp(pkg + la - 4, ".PDM") == 0)) {
            if (pdm_install_file(pkg) == 0) ok++; else pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", pkg));
            continue;
        }
        /* direct URL install: pmm install https://.../foo.zip
         * (with -dpkg/-msi the type is forced regardless of URL extension) */
        if (strncmp(pkg, "http://", 7) == 0 || strncmp(pkg, "https://", 8) == 0) {
            const char *bn = strrchr(pkg, '/');
            bn = bn ? bn + 1 : pkg;
            if (forced == 1) bn = "download.deb";
            else if (forced == 2) bn = "download.msi";
            else if (forced == 3) bn = "download.rpm";
            if (install_file(pkg, bn) == 0) ok++;
            else pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", pkg));
            continue;
        }
        /* local file install: forced -dpkg/-msi, or an existing installer file
         * (e.g. `pmm install foo.deb` / `pmm install foo.msi`) */
        if (forced == 1 || forced == 2 || forced == 3 || has_installer_ext(pkg)) {
            if (install_local_file(pkg) == 0) ok++;
            else pmm_error("%s", pmm_tr_fmt("msg.err.failed-install", pkg));
            continue;
        }
        if (install_from_registry(pkg, spec[0] ? spec : NULL, mirror.name) == 0) ok++;
    }
    free(cfg.registry_url); free(cfg.mirror_name);
    free(mirror.name); free(mirror.api_base);
    return ok == ni ? 0 : 1;
}

    if (strcmp(argv[1], "pack") == 0) {
        if (argc < 3) {
            pmm_error("%s", pmm_tr("msg.err.usage"));
            return 1;
        }
        return pdm_pack(argv[2], argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "search") == 0)
        return cmd_search(argc - 2, argv + 2);
    if (strcmp(argv[1], "cache") == 0) {
        if (argc >= 3 && strcmp(argv[2], "clean") == 0) return cmd_cache_clean();
        pmm_error("%s", pmm_tr("msg.err.usage")); return 1;
    }
    if (strcmp(argv[1], "info") == 0)
        return cmd_info(argc - 2, argv + 2);
    if (strcmp(argv[1], "verify") == 0)
        return cmd_verify(argc - 2, argv + 2);

    pmm_error("%s", pmm_tr_fmt("msg.err.unknown-cmd", argv[1]));
    return 1;
}
