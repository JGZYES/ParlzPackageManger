/* install.h */
#ifndef PMM_INSTALL_H
#define PMM_INSTALL_H

/* Download `url` into cache and install it according to file type.
 * name is the asset file name used to decide the install method. */
int install_file(const char *url, const char *name);

/* Install a local file path according to its extension — e.g.
 * `pmm install -dpkg foo.deb` (Linux) or `pmm install -msi foo.msi` (Windows). */
int install_local_file(const char *path);

/* Set by `pmm install --no-cache` to force a fresh download (drop cache file). */
extern int pmm_no_cache;

/* Install from a remote registry (apt-style multi-mirror fallback):
 * looks up package `name` in the configured registry mirrors by priority.
 * If `version` is non-NULL, fetches {registry}/{name}/{version}.json, otherwise
 * the latest pointer {registry}/{name}.json.
 * mirror_active is the name of the explicitly selected mirror, or NULL. */
int install_from_registry(const char *name, const char *version, const char *mirror_active);

#endif
