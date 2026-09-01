/* win_setup.c — minimal self-extracting installer for pmm.exe (built by CI).
 *
 * The pmm.exe payload is embedded as pmm_payload[] / pmm_payload_len (a header
 * generated from `xxd -i out/pmm.exe`, with the symbols renamed at build time).
 * On run it writes pmm.exe to %LOCALAPPDATA%\PMM\bin and adds that dir to PATH.
 *
 * Build (cross, on the ubuntu runner):
 *   x86_64-w64-mingw32-gcc -O2 -o out/pmm-setup.exe src/win_setup.c \
 *       -I. -luser32 -ladvapi32 -lopengl32  (payload.h on include path)
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "payload.h"

/* Add `dir` to HKCU\Environment\Path if not already present (user-level PATH,
 * no admin needed — mirrors a lightweight, .NET-style install). */
static void add_to_path(const char *dir) {
    HKEY key;
    char cur[32768]; DWORD sz = sizeof(cur); DWORD type = REG_SZ;
    const char *needle = dir;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD t = 0;
        if (RegQueryValueExA(key, "Path", 0, &t, (BYTE *)cur, &sz) != ERROR_SUCCESS) {
            cur[0] = 0; sz = 0; t = REG_SZ;
        }
        RegCloseKey(key);
    } else {
        cur[0] = 0; sz = 0;
    }
    if (strstr(cur, needle)) return;              /* already there */

    char bump[1] = ";";
    char next[65536];
    if (sz == 0 || cur[0] == 0) bump[0] = 0;
    /* normalize sep: if entry exists and doesn't end with ';', add one */
    _snprintf(next, sizeof(next), "%s%s%s", cur, (cur[0] && cur[strlen(cur)-1]!=';' ? ";" : bump), dir);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    RegSetValueExA(key, "Path", 0, REG_EXPAND_SZ, (const BYTE *)next, (DWORD)strlen(next) + 1);
    RegCloseKey(key);
    /* broadcast so new shells pick it up */
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
}

static void make_dirs(const char *path) {
    char tmp[4096];
    _snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '\\') { *p = 0; CreateDirectoryA(tmp, NULL); *p = '\\'; }
    CreateDirectoryA(tmp, NULL);
}

int main(void) {
    char dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, sizeof(dir));
    if (n == 0 || n >= sizeof(dir)) {
        GetModuleFileNameA(NULL, dir, sizeof(dir));
        char *s = strrchr(dir, '\\'); if (s) *s = 0;
        _snprintf(dir + strlen(dir), sizeof(dir) - strlen(dir), "\\PMM\\bin");
    } else {
        _snprintf(dir + strlen(dir), sizeof(dir) - strlen(dir), "\\PMM\\bin");
    }
    make_dirs(dir);

    char dest[MAX_PATH];
    _snprintf(dest, sizeof(dest), "%s\\pmm.exe", dir);
    FILE *f = fopen(dest, "wb");
    if (!f) { printf("PMM: cannot write %s\n", dest); return 1; }
    if (fwrite(pmm_payload, 1, pmm_payload_len, f) != pmm_payload_len) {
        fclose(f); printf("PMM: write failed\n"); return 1;
    }
    fclose(f);

    add_to_path(dir);
    printf("PMM %s installed to %s\n", PMM_SETUP_VER, dest);
    printf("PATH updated. Open a NEW terminal and run:  pmm -v\n");
    return 0;
}
