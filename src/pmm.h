/* pmm.h - shared definitions */
#ifndef PMM_H
#define PMM_H

#include <stddef.h>

/* Add PMM install dirs (~/.pmm/bin, ~/.pmm/root, and executable subdirs like
 * root/nodejs) to the user's PATH if not already present. Idempotent.
 * On Windows persists via the user registry PATH; on Unix appends to shell rc. */
void pmm_add_to_path(void);

/* Set the install location by drive letter ("D" -> D:\.pmm). An empty string
 * clears back to the user-home default. Persisted across runs. */
void pmm_set_install_drive(const char *letter);

/* Set the install location to an exact absolute path (e.g. "D:\apps\pmm" or
 * "/opt/pmm"). Config/cache/bin/root all live under it. Empty clears back. */
void pmm_set_install_path(const char *path);

/* File-association record written to the per-user registry on install. */
typedef struct {
    const char *progid;    /* e.g. "Node.JSFile" */
    const char *icon;      /* e.g. "C:\...\node.exe,0" or NULL */
    const char *cmd;       /* open command, e.g. "C:\..\node.exe" "%1" */
    const char **exts;     /* NULL-terminated list of ".js", ".mjs", ... */
} PmmAssoc;

/* Write HKCU\Software\Classes associations for the given package.
 * Backs up any pre-existing per-user entries so clear() can restore them. */
int pmm_assoc_apply(const PmmAssoc *a, const char *backup_dir);

/* Remove the associations for a package; restores backups if present. */
int pmm_assoc_clear(const PmmAssoc *a, const char *backup_dir);

/* Register an installed package in the Windows per-user "Uninstall" (Installed
 * Apps) hive, mimicking what an MSI installer writes. uninstaller is the
 * absolute path to the pmm/pdm executable used to uninstall. */
int pmm_reg_uninstall(const char *pkg, const char *ver, const char *pub,
                      const char *install_location, const char *icon,
                      const char *uninstaller, unsigned long long size_bytes);
void pmm_reg_uninstall_clear(const char *pkg);

/* Record the running executable path (argv[0], resolved to absolute) so
 * registration can point the "Uninstall" button back at us. */
void pmm_set_self_path(const char *argv0);
const char *pmm_self_path(void);

#ifndef PMM_VERSION
#define PMM_VERSION "0.1.0"   /* overridden at build time with -DPMM_VERSION="..." */
#endif

typedef enum { OS_WINDOWS, OS_LINUX, OS_MACOS, OS_UNKNOWN } PmmOS;

/* CPU architecture name string: "amd64", "aarch64", "arm64", "x86", ... */
const char *pmm_detect_arch(void);

typedef enum { HOST_GITHUB, HOST_GITLAB, HOST_GITEA, HOST_FORGEJO, HOST_AUTO, HOST_UNKNOWN } PmmHost;

PmmOS pmm_detect_os(void);
const char *pmm_os_name(PmmOS os);

/* Resolve ~/.pmm or %USERPROFILE%\.pmm into buf; returns buf or NULL. */
const char *pmm_config_dir(char *buf, size_t size);
const char *pmm_cache_dir(char *buf, size_t size);
const char *pmm_install_dir(char *buf, size_t size);

/* Find first existing config file among given extensions inside dir. */
char *pmm_find_config(const char *dir, const char *base); /* malloc'd path or NULL */

#endif
