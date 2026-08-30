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
#include <stdlib.h>
#include <string.h>

#include "pmm.h"
#include "json.h"
#include "ini.h"
#include "http.h"
#include "repo.h"
#include "install.h"
#include "pdm.h"

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

/* <flag> <repo-ish> [--host <name>] */
static int cmd_install_git(int argc, char **argv, const char *flag) {
    const char *repo = NULL;
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
    printf("pmm: host=%s repo=%s%s%s\n", host_name(host), repo,
           mirror.name ? " mirror=" : "", mirror.name ? mirror.name : "");

    PmmOS os = pmm_detect_os();
    ReleaseAsset *asset = repo_latest_asset(ctx, os);
    if (!asset && host == HOST_AUTO)
        fprintf(stderr, "pmm: could not reach a known release API for %s "
                "(tried gitea/gitlab/github shapes)\n", repo);
    repo_close(ctx);
    if (!asset) {
        if (host != HOST_AUTO)
            fprintf(stderr, "pmm: no suitable %s asset found in latest release of %s\n",
                    pmm_os_name(os), repo);
        return 1;
    }
    printf("pmm: %s -> %s (tag %s)\n", pmm_os_name(os), asset->name, asset->tag);
    int rc = install_file(asset->url, asset->name);
    repo_asset_free(asset);
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

static int cmd_list(void) {
    char dir[1024];
    pmm_install_dir(dir, sizeof(dir));
    printf("install dir: %s\n", dir);
#ifdef _WIN32
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\" 2>nul", dir);
#else
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "ls -1 \"%s\" 2>/dev/null", dir);
#endif
    return system(cmd) == 0 ? 0 : 0;
}

static void print_help(void) {
    printf("ParlzPackageManger (PMM) v%s\n\n", PMM_VERSION);
    printf("usage:\n");
    printf("  pmm install <pkg>                     install from registry mirrors (apt-style)\n");
    printf("  pmm install <file.pdm>                install a local .pdm package\n");
    printf("  pmm pack <dir> [out.pdm]              pack a folder into a .pdm\n");
    printf("  pmm pdm install|list|remove|info ...  manage .pdm packages\n");
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
        }
        argv[w++] = argv[r];
    }
    return w;
}

int main(int argc, char **argv) {
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
    if (strcmp(argv[1], "list") == 0) return cmd_list();
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
        fprintf(stderr, "pmm: usage: pmm install <pkg|file.pdm> | pmm install --git <repo>\n");
        return 1;
    }
    /* parse package name + optional version spec:
     *   pmm install nodejs==24.20.0
     *   pmm install "nodejs>=24,<25"
     *   pmm install nodejs -v 24.20.0   /  --version ">=24,<25" */
    const char *raw_pkg = NULL, *version = NULL;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "-v", 2) == 0 && a[2]) { version = a + 2; continue; }
        if (strcmp(a, "-v") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
        if (strcmp(a, "--version") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
        if (strncmp(a, "--version=", 10) == 0) { version = a + 10; continue; }
        if (!raw_pkg && a[0] != '-') { raw_pkg = a; continue; }
    }
    if (!raw_pkg) raw_pkg = argv[2];
    /* split "nodejs>=24,<25" -> pkg="nodejs", spec=">=24,<25"; else pkg only */
    char pkg[256], spec[256];
    if (version && *version) {
        strncpy(pkg, raw_pkg, sizeof(pkg) - 1);
        strncpy(spec, version, sizeof(spec) - 1);
    } else {
        const char *ops[] = { ">=", "<=", "==", "!=", ">", "<", NULL };
        const char *pos = NULL;
        for (int i = 0; ops[i]; i++) { pos = strstr(raw_pkg, ops[i]); if (pos) break; }
        if (pos) {
            size_t n = (size_t)(pos - raw_pkg);
            if (n >= sizeof(pkg)) n = sizeof(pkg) - 1;
            memcpy(pkg, raw_pkg, n); pkg[n] = '\0';
            strncpy(spec, pos, sizeof(spec) - 1);
        } else {
            strncpy(pkg, raw_pkg, sizeof(pkg) - 1);
            spec[0] = '\0';
        }
    }
    pkg[sizeof(pkg) - 1] = '\0'; spec[sizeof(spec) - 1] = '\0';
    /* local .pdm file? */
    size_t la = strlen(pkg);
    if (la > 4 && (strcmp(pkg + la - 4, ".pdm") == 0 || strcmp(pkg + la - 4, ".PDM") == 0))
        return pdm_install_file(pkg) == 0 ? 0 : 1;
    PmmConfig cfg; MirrorSel mirror;
    load_config(&cfg);
    load_mirror(&cfg, &mirror);
    /* apt-style: mirrors.first-wins by priority, active mirror if named */
    int rc = install_from_registry(pkg, spec[0] ? spec : NULL, mirror.name);
    free(cfg.registry_url); free(cfg.mirror_name);
    free(mirror.name); free(mirror.api_base);
    return rc == 0 ? 0 : 1;
}

    if (strcmp(argv[1], "pack") == 0) {
        if (argc < 3) {
            fprintf(stderr, "pmm: usage: pmm pack <dir> [output.pdm]\n");
            return 1;
        }
        return pdm_pack(argv[2], argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
    }

    if (strcmp(argv[1], "pdm") == 0) {
        /* pmm pdm install/list/remove/info  (sugar for the standalone pdm tool) */
        if (argc >= 3 && strcmp(argv[2], "install") == 0)
            return pdm_install_file(argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
        if (argc >= 3 && strcmp(argv[2], "list") == 0)  { pdm_list_installed(); return 0; }
        if (argc >= 3 && strcmp(argv[2], "remove") == 0)
            return pdm_remove(argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
        if (argc >= 3 && strcmp(argv[2], "info") == 0)
            return pdm_info(argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
        fprintf(stderr, "pmm: usage: pmm pdm install|list|remove|info ...\n");
        return 1;
    }

    fprintf(stderr, "pmm: unknown command '%s' (try 'pmm help')\n", argv[1]);
    return 1;
}
