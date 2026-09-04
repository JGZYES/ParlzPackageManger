/* pmm.c - platform & config-dir helpers */
#include "pmm.h"
#include "out.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <inttypes.h>
#define PMM_MKDIR(p) _mkdir(p)
#define PMM_POPEN_READ(cmd) _popen(cmd, "r")
#define PMM_PCLOSE_READ(p) _pclose(p)
#else
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>   /* strcasecmp/strncasecmp */
#define PMM_MKDIR(p) mkdir((p), 0755)
#define PMM_POPEN_READ(cmd) popen(cmd, "r")
#define PMM_PCLOSE_READ(p) pclose(p)
#endif

static char g_self_path[4096] = ""; /* >= PATH_MAX: realpath FORTIFY requires it */
void pmm_set_self_path(const char *argv0) {
    if (!argv0 || !*argv0) return;
#ifdef _WIN32
    if (_fullpath(g_self_path, argv0, sizeof(g_self_path)) == NULL)
        snprintf(g_self_path, sizeof(g_self_path), "%s", argv0);
#else
    /* realpath(_, NULL) allocates internally — no fixed-buffer overflow, and
     * no FORTIFY __realpath_chk abort when the caller buffer is < PATH_MAX. */
    char *rp = realpath(argv0, NULL);
    if (rp) {
        snprintf(g_self_path, sizeof(g_self_path), "%s", rp);
        free(rp);
    } else {
        snprintf(g_self_path, sizeof(g_self_path), "%s", argv0);
    }
#endif
}
const char *pmm_self_path(void) {
    return g_self_path[0] ? g_self_path : NULL;
}

PmmOS pmm_detect_os(void) {
#if defined(_WIN32)
    return OS_WINDOWS;
#elif defined(__APPLE__)
    return OS_MACOS;
#elif defined(__linux__)
    return OS_LINUX;
#else
    return OS_UNKNOWN;
#endif
}

const char *pmm_os_name(PmmOS os) {
    switch (os) {
    case OS_WINDOWS: return "windows";
    case OS_LINUX:   return "linux";
    case OS_MACOS:   return "macos";
    default:         return "unknown";
    }
}

const char *pmm_detect_arch(void) {
#if defined(_WIN32)
    /* Windows: x64 vs arm64 (ARM64 / AMD64 env) */
    const char *p = getenv("PROCESSOR_ARCHITECTURE");
    if (p && (strcmp(p, "ARM64") == 0 || strcmp(p, "arm64") == 0)) return "aarch64";
    const char *w = getenv("PROCESSOR_ARCHITEW6432");
    if (w && (strcmp(w, "ARM64") == 0)) return "aarch64";
    return "amd64";
#elif defined(__aarch64__) || defined(__arm64__)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "amd64";
#elif defined(__i386__) || defined(__i686__) || defined(_M_IX86)
    return "x86";
#else
    return "any";
#endif
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
            PMM_MKDIR(tmp);
            *p = c;
        }
    }
    PMM_MKDIR(tmp);
}

static const char *home_dir(void) {
    const char *h = getenv("USERPROFILE"); /* windows */
    if (!h || !*h) h = getenv("HOME");
    return (h && *h) ? h : ".";
}

/* ---- install-location overrides ----
 * Either a drive letter ("D" -> D:\.pmm) via -p<drive>, or an exact path
 * ("D:\apps\pmm" / "/opt/pmm") via -p <path>. The path override wins. */
static char g_drive[4] = "";   /* "D", or "" = use home */
static char g_base_path[1024] = ""; /* exact install base, or "" */

static const char *drive_state_path(char *buf, size_t size) {
    snprintf(buf, size, "%s/.pmm/install-drive", home_dir());
    return buf;
}

void pmm_set_install_drive(const char *letter) {
    if (!letter || !*letter) { g_drive[0] = '\0'; return; }
    char c = (char)((*letter >= 'a' && *letter <= 'z') ? *letter - 'a' + 'A' : *letter);
    if (c < 'A' || c > 'Z') return; /* only a single A-Z drive letter */
    g_drive[0] = c;
    g_drive[1] = '\0';
    char state[1024];
    drive_state_path(state, sizeof(state));
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.pmm", home_dir());
    mkdir_p(dir);
    FILE *f = fopen(state, "w");
    if (f) { fprintf(f, "%c", c); fclose(f); }
    pmm_success("install drive set to %c:\n", c);
}

void pmm_set_install_path(const char *path) {
    if (!path || !*path) { g_base_path[0] = '\0'; return; }
    snprintf(g_base_path, sizeof(g_base_path), "%s", path);
    /* strip trailing slashes */
    size_t l = strlen(g_base_path);
    while (l > 1 && (g_base_path[l-1] == '/' || g_base_path[l-1] == '\\')) g_base_path[--l] = '\0';
    mkdir_p(g_base_path);
    pmm_success("install path set to %s\n", g_base_path);
}
int pmm_flat_mode(void) { return g_base_path[0] ? 1 : 0; }

/* Resolve the .pmm base dir: explicit drive override > persisted drive >
 * home/.pmm (when that drive is picked) > DEFAULT = D:\.pmm.
 * Defaulting to D:\ spares the C: drive (user's primary diskspace concern);
 * use -pc to go back to the user-home .pmm, or -p<drive> for any other drive. */
static char home_drive_letter(void) {
    const char *h = home_dir();
    if (h[0] && h[1] == ':' && ((h[0] >= 'A' && h[0] <= 'Z') || (h[0] >= 'a' && h[0] <= 'z')))
        return (char)((h[0] >= 'a' && h[0] <= 'z') ? h[0] - 'a' + 'A' : h[0]);
    return 0; /* not a drive-letter home (unix) */
}

static const char *pmm_base_dir(char *buf, size_t size) {
    if (g_base_path[0]) {                                     /* -p <exact path> */
        snprintf(buf, size, "%s", g_base_path);
        return buf;
    }
    char drive[4] = "";
    if (g_drive[0]) {
        drive[0] = g_drive[0];
    } else {
        char state[1024];
        drive_state_path(state, sizeof(state));
        FILE *f = fopen(state, "r");
        if (f) {
            char c = (char)fgetc(f);
            fclose(f);
            if (c >= 'A' && c <= 'Z') drive[0] = c;
            else if (c >= 'a' && c <= 'z') drive[0] = (char)(c - 'a' + 'A');
        }
    }
    char hd = home_drive_letter();
    if (drive[0] && hd && drive[0] != hd) {
        snprintf(buf, size, "%c:\\.pmm", drive[0]);           /* -p<other> */
    } else if (drive[0] && hd && drive[0] == hd) {
        snprintf(buf, size, "%s/.pmm", home_dir());           /* -pc -> home */
    } else {
#ifdef _WIN32
        snprintf(buf, size, "D:\\.pmm");                      /* Windows DEFAULT -> D: */
#else
        snprintf(buf, size, "%s/.pmm", home_dir());           /* unix default -> ~/.pmm */
#endif
    }
    return buf;
}

const char *pmm_config_dir(char *buf, size_t size) {
    pmm_base_dir(buf, size);
    mkdir_p(buf);
    return buf;
}

const char *pmm_cache_dir(char *buf, size_t size) {
    char base[1024];
    pmm_base_dir(base, sizeof(base));
    snprintf(buf, size, "%s/cache", base);
    mkdir_p(buf);
    return buf;
}

const char *pmm_install_dir(char *buf, size_t size) {
    const char *env = getenv("PMM_INSTALL_DIR");
    if (env && *env) { snprintf(buf, size, "%s", env); }
    else if (g_base_path[0]) {
        snprintf(buf, size, "%s", g_base_path);   /* -p <path>: flat install into the specified dir */
    } else {
#ifdef _WIN32
        char base[1024];
        pmm_base_dir(base, sizeof(base));
        snprintf(buf, size, "%s/bin", base);      /* Windows default -> <base>\bin */
#else
        snprintf(buf, size, "/usr/local/bin");     /* Linux/macOS default -> system-wide (on PATH) */
#endif
    }
    mkdir_p(buf);
    return buf;
}

char *pmm_find_config(const char *dir, const char *base) {
    static const char *exts[] = { ".json", ".ini", ".conf", NULL };
    static char out[1024];
    for (int i = 0; exts[i]; i++) {
        snprintf(out, sizeof(out), "%s/%s%s", dir, base, exts[i]);
        struct stat st;
        if (stat(out, &st) == 0 && !S_ISDIR(st.st_mode))
            return out;
    }
    return NULL;
}

/* ---------- PATH auto-add ---------- */

static int dir_has_exec(const char *dir) {
#ifdef _WIN32
    char patt[1300];
    snprintf(patt, sizeof(patt), "%s\\*", dir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(patt, &fd);
    if (h == -1) return 0;
    int found = 0;
    do {
        if (fd.attrib & _A_SUBDIR) continue;
        size_t l = strlen(fd.name);
        if ((l > 4 && strcasecmp(fd.name + l - 4, ".exe") == 0) ||
            (l > 4 && strcasecmp(fd.name + l - 4, ".cmd") == 0) ||
            (l > 4 && strcasecmp(fd.name + l - 4, ".bat") == 0)) { found = 1; break; }
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
    return found;
#else
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char full[1300];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && !S_ISDIR(st.st_mode)) { found = 1; break; }
    }
    closedir(d);
    return found;
#endif
}

/* case-insensitive "dir" present in a ';'-separated PATH (win) / ':' (unix) */
static int path_has(const char *path, const char *dir, char sep) {
    if (!path || !*path) return 0;
    size_t dl = strlen(dir);
    const char *p = path;
    for (;;) {
        const char *e = strchr(p, sep);
        size_t n = e ? (size_t)(e - p) : strlen(p);
        while (n && (p[n-1]==' '||p[n-1]=='"')) n--;
        const char *q = p;
        while (*q==' '||*q=='"') q++;
        if (n && strncasecmp(q, dir, dl) == 0 && (n == dl)) return 1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

/* Add a dir to the candidate list if not already listed. */
static void add_path_candidate(char list[][1200], int *n, int max, const char *dir) {
    for (int i = 0; i < *n; i++)
        if (strcmp(list[i], dir) == 0) return;
    if (*n < max) snprintf(list[(*n)++], 1200, "%s", dir);
}

void pmm_add_to_path(void) {
    char home[1024];
    pmm_config_dir(home, sizeof(home));
    char base[1024];
    if (pmm_flat_mode()) snprintf(base, sizeof(base), "%s", home);   /* -p <path> */
    else snprintf(base, sizeof(base), "%s/root", home);

    /* collect dirs: ~/.pmm/bin, ~/.pmm/root, and root subdirs holding executables */
    char list[160][1200];
    int n = 0;
    char bin[1100];
    snprintf(bin, sizeof(bin), "%s/bin", home);
    mkdir_p(bin);
    snprintf(list[n++], sizeof(list[0]), "%s", bin);
    snprintf(list[n++], sizeof(list[0]), "%s", base);
    mkdir_p(base);

#ifdef _WIN32
    {
        char patt[1600];
        snprintf(patt, sizeof(patt), "%s\\*", base);
        char cmd[1700];
        snprintf(cmd, sizeof(cmd), "dir /b /ad \"%s\" 2>nul", patt);
        FILE *f = PMM_POPEN_READ(cmd);
        if (f) {
            char sub[1200];
            while (fgets(sub, sizeof(sub), f)) {
                size_t l = strlen(sub);
                while (l && (sub[l-1]=='\r'||sub[l-1]=='\n')) sub[--l] = 0;
                if (!l) continue;
                char full[1300];
                snprintf(full, sizeof(full), "%s/%s", base, sub);
                if (dir_has_exec(full)) {
                    add_path_candidate(list, &n, 160, full);
                } else {
                    /* depth-2: pkg/cmd convention (e.g. git/cmd/git.exe) */
                    char cmddir[1400];
                    snprintf(cmddir, sizeof(cmddir), "%s/cmd", full);
                    if (dir_has_exec(cmddir))
                        add_path_candidate(list, &n, 160, cmddir);
                }
            }
            PMM_PCLOSE_READ(f);
        }
    }
#else
    {
        char cmd[1700];
        snprintf(cmd, sizeof(cmd), "ls -1 \"%s\" 2>/dev/null", base);
        FILE *f = PMM_POPEN_READ(cmd);
        if (f) {
            char sub[1200];
            while (fgets(sub, sizeof(sub), f)) {
                size_t l = strlen(sub);
                while (l && (sub[l-1]=='\r'||sub[l-1]=='\n')) sub[--l] = 0;
                char full[1300];
                snprintf(full, sizeof(full), "%s/%s", base, sub);
                if (dir_has_exec(full)) {
                    add_path_candidate(list, &n, 160, full);
                } else {
                    char cmddir[1400];
                    snprintf(cmddir, sizeof(cmddir), "%s/cmd", full);
                    if (dir_has_exec(cmddir))
                        add_path_candidate(list, &n, 160, cmddir);
                }
            }
            PMM_PCLOSE_READ(f);
        }
    }
#endif

    /* read current user PATH */
    char cur[16384] = "";
#ifdef _WIN32
    {
        FILE *f = PMM_POPEN_READ("reg query \"HKCU\\Environment\" /v Path 2>nul");
        if (f) {
            char line[8192];
            char *val = NULL;
            while (fgets(line, sizeof(line), f)) {
                char *r = strstr(line, "REG_SZ");
                if (r) { val = r + 6; while (*val==' '||*val=='\t'||*val=='\r'||*val=='\n') val++; break; }
            }
            PMM_PCLOSE_READ(f);
            if (val) {
                size_t v = strlen(val);
                while (v && (val[v-1]=='\r'||val[v-1]=='\n')) val[--v] = 0;
                snprintf(cur, sizeof(cur), "%s", val);
            }
        }
    }
#else
    {
        const char *env = getenv("PATH");
        if (env) snprintf(cur, sizeof(cur), "%s", env);
    }
#endif

    /* append missing dirs (bounds-safe: never let size_t underflow) */
    char next[17000] = "";
    snprintf(next, sizeof(next), "%s", cur);
    int added = 0;
    for (int i = 0; i < n; i++) {
        if (!list[i][0]) continue;
        if (path_has(next, list[i], ';') || path_has(next, list[i], ':')) continue;
        size_t l = strlen(next);
        /* ensure a separator between entries */
        if (l && next[l-1] != ';' && next[l-1] != ':' && l + 1 < sizeof(next)) {
            next[l] = ';'; next[l+1] = '\0'; l++;
        }
        size_t room = sizeof(next) - l - 1;
        if (room <= 0) break;                 /* no space left: stop */
        snprintf(next + l, room + 1, "%s", list[i]);
        added++;
        pmm_info("adding to PATH: %s\n", list[i]);
    }
    if (!added) { pmm_info("PATH already contains PMM dirs\n"); return; }

#ifdef _WIN32
    {
        char cmd[17001];
        /* drive letters and ';' are fine for setx; quote the value */
        snprintf(cmd, sizeof(cmd), "setx Path \"%s\" >nul 2>&1", next);
        system(cmd);
        pmm_success("PATH updated for new shells (reopen a terminal or run: setx)  (user-level)\n");
    }
#else
    {
        const char *she = getenv("SHELL");
        const char *rc = "~/.bashrc";
        char rcp[1100];
        snprintf(rcp, sizeof(rcp), "%s/.bashrc", home_dir());
        FILE *f = fopen(rcp, "a");
        if (f) {
            fprintf(f, "\n# PMM PATH\n");
            for (int i = 0; i < n; i++) if (list[i][0]) fprintf(f, "export PATH=\"%s:$PATH\"\n", list[i]);
            fclose(f);
        }
        (void)she;
        pmm_success("updated %s (reload your shell)\n", rcp);
    }
#endif
}

/* ---------- register installed packages in the Windows "apps" (Uninstall) ----------
 * Mimics what an MSI installer writes: HKCU\Software\Microsoft\
 * Windows\CurrentVersion\Uninstall\<pkg>, so the package appears in
 * "Installed apps" / Add-Remove Programs and can be uninstalled via pmm. */

/* Escape a string for a single-quoted PowerShell literal (backslashes are
 * literal there; only a single quote needs doubling: ' -> ''). */
static void ps_squote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (const char *p = in; *p && o + 3 < outsz; p++) {
        if (*p == '\'') { out[o++] = '\''; out[o++] = '\''; }
        out[o++] = *p;
    }
    out[o] = '\0';
}

int pmm_reg_uninstall(const char *pkg, const char *ver, const char *pub,
                      const char *install_location, const char *icon,
                      const char *uninstaller, unsigned long long size_bytes) {
#ifndef _WIN32
    (void)pkg; (void)ver; (void)pub; (void)install_location; (void)icon;
    (void)uninstaller; (void)size_bytes;
    return 0; /* only Windows registers software in the Uninstall hive */
#else
    if (!pkg || !*pkg) return -1;
    char base[1024];
    pmm_config_dir(base, sizeof(base));
    char cfg[1100];
    snprintf(cfg, sizeof(cfg), "%s\\cache", base);
    mkdir_p(base); mkdir_p(cfg);
    char ps1[1300];
    snprintf(ps1, sizeof(ps1), "%s\\pmm-uninst-%s.ps1", cfg, pkg);
    /* PowerShell-escape the values */
    char e_nm[700], e_ver[700], e_pub[700], e_loc[2400], e_icon[2600], e_un[4200];
    ps_squote(pkg, e_nm, sizeof(e_nm));
    ps_squote(ver ? ver : "", e_ver, sizeof(e_ver));
    ps_squote(pub ? pub : "", e_pub, sizeof(e_pub));
    ps_squote(install_location ? install_location : "", e_loc, sizeof(e_loc));
    ps_squote(icon ? icon : "", e_icon, sizeof(e_icon));
    char uninst[2048];
    if (uninstaller && *uninstaller) snprintf(uninst, sizeof(uninst), "\"%s\" remove %s", uninstaller, pkg);
    else snprintf(uninst, sizeof(uninst), "pmm remove %s", pkg);
    ps_squote(uninst, e_un, sizeof(e_un));

    FILE *f = fopen(ps1, "wb");
    if (!f) return -1;
    fprintf(f, "$ErrorActionPreference='SilentlyContinue'\r\n");
    fprintf(f, "$k='HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s'\r\n", pkg);
    fprintf(f, "New-Item -Path $k -Force | Out-Null\r\n");
    fprintf(f, "Set-ItemProperty -Path $k -Name DisplayName -Value '%s' -Force\r\n", e_nm);
    if (ver && *ver) fprintf(f, "Set-ItemProperty -Path $k -Name DisplayVersion -Value '%s' -Force\r\n", e_ver);
    if (pub && *pub) fprintf(f, "Set-ItemProperty -Path $k -Name Publisher -Value '%s' -Force\r\n", e_pub);
    if (icon && *icon) fprintf(f, "Set-ItemProperty -Path $k -Name DisplayIcon -Value '%s' -Force\r\n", e_icon);
    fprintf(f, "Set-ItemProperty -Path $k -Name UninstallString -Value '%s' -Force\r\n", e_un);
    if (install_location && *install_location)
        fprintf(f, "Set-ItemProperty -Path $k -Name InstallLocation -Value '%s' -Force\r\n", e_loc);
    fprintf(f, "New-ItemProperty -Path $k -Name EstimatedSize -Value %llu -PropertyType DWord -Force | Out-Null\r\n",
            (unsigned long long)(size_bytes / 1024ULL));
    { /* InstallDate YYYYMMDD */
        time_t t = time(NULL); struct tm *tm = localtime(&t);
        char date[32];
        if (tm) snprintf(date, sizeof(date), "%04d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        else snprintf(date, sizeof(date), "0");
        fprintf(f, "Set-ItemProperty -Path $k -Name InstallDate -Value '%s' -Force\r\n", date);
    }
    fprintf(f, "New-ItemProperty -Path $k -Name NoModify -Value 1 -PropertyType DWord -Force | Out-Null\r\n");
    fprintf(f, "New-ItemProperty -Path $k -Name NoRepair -Value 1 -PropertyType DWord -Force | Out-Null\r\n");
    fclose(f);

    char cmd[2600];
    snprintf(cmd, sizeof(cmd),
             "%%SystemRoot%%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" >nul 2>&1",
             ps1);
    int rc = system(cmd);
    remove(ps1);
    if (rc != 0)
        pmm_warn("warning: could not register '%s' in Installed Apps\n", pkg);
    else
        pmm_success("registered '%s' %s in Installed Apps (uninstall via pmm remove %s)\n",
               pkg, ver ? ver : "", pkg);
    return rc == 0 ? 0 : -1;
#endif
}

void pmm_reg_uninstall_clear(const char *pkg) {
#ifndef _WIN32
    (void)pkg;
#else
    if (!pkg || !*pkg) return;
    char cmd[2200];
    snprintf(cmd, sizeof(cmd),
             "%%SystemRoot%%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -NoProfile -Command \"Remove-Item -Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s' -Recurse -Force -ErrorAction SilentlyContinue\" 2>nul",
             pkg);
    system(cmd);
    pmm_success("removed '%s' from Installed Apps\n", pkg);
#endif
}
