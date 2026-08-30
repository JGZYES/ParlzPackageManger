/* pdm_main.c - standalone `pdm` tool (pack/install/remove/info/list)
 *
 * usage:
 *   pdm pack <dir> [output.pdm]   pack a folder (with pdm-control) into .pdm
 *   pdm install <file.pdm>        install local .pdm
 *   pdm info <file.pdm>           show package control info
 *   pdm list                      list installed packages
 *   pdm remove <name>             remove installed package
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pdm.h"
#include "pmm.h"
#include "install.h"

static void usage(void) {
    printf("pdm - Parlz .pdm package tool\n\n"
           "usage:\n"
           "  pdm pack <dir> [output.pdm]   pack folder (needs pdm-control) into .pdm\n"
           "  pdm install <file.pdm>        install local .pdm\n"
           "  pdm install <pkg> [-v <ver>]  install from registry (pmm mirror)\n"
           "  pdm info <file.pdm>           show control info\n"
           "  pdm list                      list installed packages\n"
           "  pdm remove <name>             remove installed package\n"
           "options: -p<drive>  install under <DRIVE>:\\.pdm (e.g. -pd -> D:\\.pdm)\n");
}

/* Parse "-p<drive>" / "-p <drive>" and compact argv; records the drive so
 * pdm installs under <DRIVE>:\.pmm. */
static int consume_drive_flag(int argc, char **argv) {
    int w = 1;
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
                r++;
                continue;
            }
        }
        argv[w++] = argv[r];
    }
    return w;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 0; }
    pmm_set_self_path(argv[0]);
    argc = consume_drive_flag(argc, argv);
    if (argc < 2) { usage(); return 0; }
    if (strcmp(argv[1], "pack") == 0) {
        if (argc < 3) { fprintf(stderr, "pdm: usage: pdm pack <dir> [output.pdm]\n"); return 1; }
        return pdm_pack(argv[2], argc >= 4 ? argv[3] : NULL) == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "install") == 0 || strcmp(argv[1], "-i") == 0) {
        if (argc < 3) { fprintf(stderr, "pdm: usage: pdm install <file.pdm|pkg> [-v ver]\n"); return 1; }
        /* parse [pkg] [ -v <ver> ] ; pkg may be "nodejs==24.20.0" / "nodejs>=24,<25" */
        const char *raw = NULL, *version = NULL;
        for (int i = 2; i < argc; i++) {
            const char *a = argv[i];
            if (strncmp(a, "-v", 2) == 0 && a[2]) { version = a + 2; continue; }
            if (strcmp(a, "-v") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
            if (strcmp(a, "--version") == 0 && i + 1 < argc) { version = argv[++i]; continue; }
            if (strncmp(a, "--version=", 10) == 0) { version = a + 10; continue; }
            if (!raw && a[0] != '-') { raw = a; continue; }
        }
        if (!raw) raw = argv[2];
        char pkg[256], spec[256];
        if (version && *version) {
            strncpy(pkg, raw, sizeof(pkg) - 1);
            strncpy(spec, version, sizeof(spec) - 1);
        } else {
            const char *ops[] = { ">=", "<=", "==", "!=", ">", "<", NULL };
            const char *pos = NULL;
            for (int i = 0; ops[i]; i++) { pos = strstr(raw, ops[i]); if (pos) break; }
            if (pos) { size_t n = (size_t)(pos - raw); if (n >= sizeof(pkg)) n = sizeof(pkg) - 1; memcpy(pkg, raw, n); pkg[n] = '\0'; strncpy(spec, pos, sizeof(spec) - 1); }
            else { strncpy(pkg, raw, sizeof(pkg) - 1); spec[0] = '\0'; }
        }
        pkg[sizeof(pkg) - 1] = '\0'; spec[sizeof(spec) - 1] = '\0';
        size_t l = strlen(pkg);
        if (l > 4 && (strcmp(pkg + l - 4, ".pdm") == 0 || strcmp(pkg + l - 4, ".PDM") == 0))
            return pdm_install_file(pkg) == 0 ? 0 : 1;
        /* otherwise treat as a registry package name */
        return install_from_registry(pkg, spec[0] ? spec : NULL, NULL) == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) { fprintf(stderr, "pdm: usage: pdm info <file.pdm>\n"); return 1; }
        return pdm_info(argv[2]) == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "-l") == 0) {
        pdm_list_installed();
        return 0;
    }
    if (strcmp(argv[1], "remove") == 0 || strcmp(argv[1], "-r") == 0) {
        if (argc < 3) { fprintf(stderr, "pdm: usage: pdm remove <name>\n"); return 1; }
        return pdm_remove(argv[2]) == 0 ? 0 : 1;
    }
    usage();
    return 1;
}
