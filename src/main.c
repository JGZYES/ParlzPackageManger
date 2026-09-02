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
#include "json.h"
#include "ini.h"
#include "http.h"
#include "repo.h"
#include "install.h"
#include "pdm.h"
#include "mirrors.h"
#include "sha256.h"
#include "sha1.h"

typedef struct {
    char *registry_url;   /* package registry base URL */
    char *mirror_name;    /* active mirror name ("" = none) */
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
    if (argc < 1) { fprintf(stderr, "pmm: usage: pmm search <keyword>\n"); return 1; }
    int status = 0;
    char *body = registry_fetch("packages.json", &status);
    if (!body) { fprintf(stderr, "pmm: no registry index (packages.json) available\n"); return 1; }
    JsonValue *root = json_parse(body);
    free(body);
    if (!root || root->type != JSON_ARRAY) { fprintf(stderr, "pmm: bad registry index\n"); json_free(root); return 1; }
    const char *kw = argv[0];
    int found = 0;
    for (int i = 0; i < root->count; i++) {
        JsonValue *v = json_at(root, i);
        const char *name = (v && v->type == JSON_STRING) ? v->string : NULL;
        if (!name) continue;
        if (ci_contains(name, kw)) { printf("  %s\n", name); found++; }
    }
    json_free(root);
    if (!found) printf("pmm: no package matches '%s'\n", kw);
    return 0;
}

/* pmm info <package|file.pdm> — registry package info, or a local .pdm's control. */
static int cmd_info(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "pmm: usage: pmm info <package|file.pdm>\n"); return 1; }
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
    if (!body) { fprintf(stderr, "pmm: package '%s' not found in registry\n", pkg); return 1; }
    JsonValue *root = json_parse(body);
    free(body);
    if (!root || root->type != JSON_OBJECT) { fprintf(stderr, "pmm: bad registry entry '%s'\n", pkg); json_free(root); return 1; }
    const char *name = json_str(root, "name");
    const char *ver  = json_str(root, "version");
    printf("pmm: %s%s%s\n", name ? name : pkg, ver ? " " : "", ver ? ver : "");
    JsonValue *vr = json_get(root, "variants");
    if (vr && vr->count > 0) {
        for (int i = 0; i < vr->count; i++) {
            JsonValue *v = json_at(vr, i);
            if (!v) continue;
            const char *vv = json_str(v, "version");
            const char *osn = json_str(v, "os");
            const char *archn = json_str(v, "arch");
            printf("  %s   %s/%s\n", vv ? vv : "?", osn ? osn : "-", archn ? archn : "-");
        }
    } else {
        const char *u = json_str(root, "url");
        printf("  url: %s\n", u ? u : "-");
    }
    json_free(root);
    return 0;
}

/* pmm verify <file> — print sha256 (and sha1) of a downloaded package file. */
static int cmd_verify(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "pmm: usage: pmm verify <file>\n"); return 1; }
    const char *file = argv[0];
    char hex[128];
    if (pmm_sha256_file(file, hex) == 0) printf("pmm: sha256  %s  %s\n", hex, file);
    else { fprintf(stderr, "pmm: cannot read %s\n", file); return 1; }
    if (pmm_sha1_file(file, hex) == 0) printf("pmm: sha1    %s  %s\n", hex, file);
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
    printf("pmm: cleaning cache %s\n", dir);
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
                fprintf(stderr, "pmm: unknown host '%s'\n", argv[i]);
                return 1;
            }
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--branch") == 0) && i + 1 < argc) {
            branch = argv[++i];
        } else if (!repo) {
            repo = argv[i];
        }
    }
    if (!repo) {
        fprintf(stderr,
            "pmm: usage:\n"
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
    printf("pmm: host=%s repo=%s%s%s\n", host_name(host), repo,
           mirror.name ? " mirror=" : "", mirror.name ? mirror.name : "");

    PmmOS os = pmm_detect_os();
    ReleaseAsset *asset = repo_latest_asset(ctx, os);
    if (!asset && host == HOST_AUTO)
        fprintf(stderr, "pmm: could not reach a known release API for %s "
                "(tried gitea/gitlab/github shapes)\n", repo);
    if (!asset) {
        if (host != HOST_AUTO)
            fprintf(stderr, "pmm: no suitable %s asset found in latest release of %s\n",
                    pmm_os_name(os), repo);
        repo_close(ctx);
        return 1;
    }
    printf("pmm: %s -> %s (tag %s)\n", pmm_os_name(os), asset->name, asset->tag);
    int rc = install_file(asset->url, asset->name);

    /* fallback: the first pick may have been a cross-platform/wrong asset
     * (e.g. fzf's Android binary named without an OS hint). Re-fetch a clearly
     * os-named one and install that instead. */
    if (rc != 0 && host != HOST_AUTO) {
        const char *sub = (os == OS_LINUX) ? "linux" : (os == OS_MACOS) ? "darwin" : NULL;
        if (sub) {
            ReleaseAsset *fb = repo_asset_matching(ctx, os, sub);
            if (fb && strcmp(fb->name, asset->name) != 0) {
                printf("pmm: retrying with %s (%s)\n", fb->name, pmm_os_name(os));
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
    if (strcmp(argv[0], "add") == 0 && argc >= 3) {
        FILE *f = fopen(path, "a");
        if (!f) { fprintf(stderr, "pmm: cannot write %s\n", path); return 1; }
        fprintf(f, "\n[%s]\napi = %s\n", argv[1], argv[2]);
        fclose(f);
        printf("pmm: mirror '%s' added\n", argv[1]);
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
        if (!wf) { fprintf(stderr, "pmm: cannot write %s\n", cfgpath); return 1; }
        int wrote = 0;
        for (int i = 0; i < n; i++) fputs(lines[i], wf);
        fprintf(wf, "mirror = %s\n", argv[1]);
        (void)wrote;
        fclose(wf);
        printf("pmm: active mirror set to '%s'\n", argv[1]);
        return 0;
    }
    if (strcmp(argv[0], "remove") == 0 && argc >= 2) {
        Ini *ini = ini_load(path);
        if (!ini) { fprintf(stderr, "pmm: no mirror file\n"); return 1; }
        FILE *f = fopen(path, "w");
        if (!f) { fprintf(stderr, "pmm: cannot write %s\n", path); ini_free(ini); return 1; }
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
        printf("pmm: mirror '%s' removed\n", argv[1]);
        return 0;
    }
    fprintf(stderr, "pmm: usage: pmm mirror list|add|use|remove ...\n");
    return 1;
}

static void print_help(void) {
    printf("ParlzPackageManger (PMM) v%s\n\n", PMM_VERSION);
    printf("usage:\n");
    printf("  pmm install <pkg>                     install from registry mirrors (apt-style)\n");
    printf("  pmm install <file.pdm>                install a local .pdm package\n");
    printf("  pmm install <pkg> [pkg2 ...]          install several packages\n");
    printf("  pmm install <file.deb|file.msi|...>   install a local package file\n");
    printf("  pmm install -dpkg <x.deb>             install a .deb (Linux)\n");
    printf("  pmm install -msi <x.msi>              install an .msi (Windows)\n");
    printf("  pmm install <https://...>             install a file from a URL\n");
    printf("  pmm search <keyword>                  list registry packages matching keyword\n");
    printf("  pmm info <package|file.pdm>           registry/local package info\n");
    printf("  pmm verify <file>                     print sha256/sha1 of a package file\n");
    printf("  pmm pack <dir> [out.pdm]              pack a folder into a .pdm\n");
    printf("  pmm list                              list installed packages\n");
    printf("  pmm remove <pkg>                      uninstall a package\n");
    printf("  pmm self-update                       update pmm (auto os/arch)\n");
    printf("  pmm install --git <repo-url.git>      any git host, API auto-detected\n");
    printf("  pmm install --github <owner/repo>     GitHub latest release\n");
    printf("  pmm install --gitlab <owner/repo>     GitLab latest release\n");
    printf("  pmm install --gitea <url/owner/repo>  Gitea/Forgejo (incl. self-hosted)\n");
    printf("       [--host github|gitlab|gitea|forgejo]\n");
    printf("  pmm mirror list|add <n> <api>|use <n>|remove <n>\n");
    printf("  pmm list                             list installed files\n");
    printf("  pmm version | help\n\n");
    printf("options:\n");
    printf("  -p<drive>    install under <DRIVE>:\\\\.pmm (e.g. -pd -> D:\\\\.pmm, -pc -> C:\\\\.pmm)\n");
    printf("               第一个 -p 会被记住，后续命令无需再写；用 -pc 切回 C 盘\n\n");
    printf("config: <base>/pmm.json | pmm.ini | pmm.conf  (base = <drive>:\\\\.pmm 或 ~/.pmm)\n");
    printf("mirrors: <base>/mirror.ini | mirror.conf\n");
    printf("asset mapping: windows=exe/msi/zip/7z  linux=deb/rpm/appimage/tar.*  macos=dmg/pkg\n");
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

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   /* UTF-8 output so Chinese help/version isn't mojibake */
    setvbuf(stdout, NULL, _IONBF, 0);
#endif
    if (argc < 2) { print_help(); return 0; }
    pmm_set_self_path(argv[0]);
    argc = consume_drive_flag(argc, argv);
    if (argc < 2) { print_help(); return 0; }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help(); return 0;
    }
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("pmm %s (%s) [%s]\n", PMM_VERSION, pmm_os_name(pmm_detect_os()), pmm_detect_arch());
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) { pdm_list_installed(); return 0; }
    if (strcmp(argv[1], "mirror") == 0) return cmd_mirror(argc - 2, argv + 2);
    /* pmm remove <pkg>  (also used by the Windows "Uninstall" registration) */
    if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) { fprintf(stderr, "pmm: usage: pmm remove <pkg>\n"); return 1; }
        return pdm_remove(argv[2]) == 0 ? 0 : 1;
    }
    /* pmm self-update: install the latest 'pmm' tool package (auto os+arch) */
    if (strcmp(argv[1], "self-update") == 0 || strcmp(argv[1], "update") == 0) {
        PmmConfig cfg; MirrorSel mirror;
        load_config(&cfg);
        load_mirror(&cfg, &mirror);
        printf("pmm: self-update -> installing latest pmm (%s/%s)\n",
               pmm_os_name(pmm_detect_os()), pmm_detect_arch());
        int rc = install_from_registry("pmm", NULL, mirror.name);
        free(cfg.registry_url); free(cfg.mirror_name);
        free(mirror.name); free(mirror.api_base);
        if (rc == 0)
            printf("pmm: updated. New tools are under %s/root/bin (pmm/pdm). Re-run from there, or add it to PATH.\n",
                   pmm_config_dir((char[1024]){0}, 1024));
        return rc == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc >= 3 && (strcmp(argv[2], "--git") == 0 || strcmp(argv[2], "--github") == 0 ||
                          strcmp(argv[2], "--gitlab") == 0 || strcmp(argv[2], "--gitea") == 0 ||
                          strcmp(argv[2], "--forgejo") == 0))
            return cmd_install_git(argc - 3, argv + 3, argv[2] + 2);
    if (argc < 3) {
        fprintf(stderr, "pmm: usage: pmm install <pkg|file.deb|file.msi|file.pdm> [pkg2 ...]\n"
                        "           | pmm install -dpkg <x.deb>   install a .deb (Linux)\n"
                        "           | pmm install -msi <x.msi>    install an .msi (Windows)\n"
                        "           | pmm install --git <repo>\n");
        return 1;
    }
    /* collect one or more package arguments (each may carry an inline version
     * spec like nodejs>=24,<25); optional -v/--version applies to a single one */
    const char *version = NULL;
    int forced = 0;   /* 1 = -dpkg, 2 = -msi (install a local package by type) */
    const char *items[64]; int ni = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--no-cache") == 0) { pmm_no_cache = 1; continue; }
        if (strcmp(a, "-dpkg") == 0) { forced = 1; if (i + 1 < argc) items[ni++] = argv[++i]; continue; }
        if (strcmp(a, "-msi") == 0)  { forced = 2; if (i + 1 < argc) items[ni++] = argv[++i]; continue; }
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
            if (pdm_install_file(pkg) == 0) ok++; else fprintf(stderr, "pmm: failed to install %s\n", pkg);
            continue;
        }
        /* direct URL install: pmm install https://.../foo.zip
         * (with -dpkg/-msi the type is forced regardless of URL extension) */
        if (strncmp(pkg, "http://", 7) == 0 || strncmp(pkg, "https://", 8) == 0) {
            const char *bn = strrchr(pkg, '/');
            bn = bn ? bn + 1 : pkg;
            if (forced == 1) bn = "download.deb";
            else if (forced == 2) bn = "download.msi";
            if (install_file(pkg, bn) == 0) ok++;
            else fprintf(stderr, "pmm: failed to install %s\n", pkg);
            continue;
        }
        /* local file install: forced -dpkg/-msi, or an existing installer file
         * (e.g. `pmm install foo.deb` / `pmm install foo.msi`) */
        if (forced == 1 || forced == 2 || has_installer_ext(pkg)) {
            if (install_local_file(pkg) == 0) ok++;
            else fprintf(stderr, "pmm: failed to install %s\n", pkg);
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
            fprintf(stderr, "pmm: usage: pmm pack <dir> [output.pdm]\n");
            return 1;
        }
        return pdm_pack(argv[2], argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "search") == 0)
        return cmd_search(argc - 2, argv + 2);
    if (strcmp(argv[1], "cache") == 0) {
        if (argc >= 3 && strcmp(argv[2], "clean") == 0) return cmd_cache_clean();
        fprintf(stderr, "pmm: usage: pmm cache clean\n"); return 1;
    }
    if (strcmp(argv[1], "info") == 0)
        return cmd_info(argc - 2, argv + 2);
    if (strcmp(argv[1], "verify") == 0)
        return cmd_verify(argc - 2, argv + 2);

    fprintf(stderr, "pmm: unknown command '%s' (try 'pmm help')\n", argv[1]);
    return 1;
}
