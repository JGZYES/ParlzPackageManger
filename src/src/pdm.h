/* pdm.h - .pdm package format (deb-like)
 *
 * A .pdm file is a tar archive with three members:
 *   control.tar.gz  -> contains "pdm-control" text file (Package/Version/...)
 *   data.tar.gz     -> the package payload, extracted into the pm root
 *   sha256sums      -> "<hex>  control.tar.gz" lines for both members
 *
 * pdm-control fields (KEY: value lines):
 *   Package: name
 *   Version: 1.0.0
 *   Architecture: all | windows | linux | macos
 *   Description: text
 *   Maintainer: text
 *
 * The folder passed to `pdm pack ./dir` must contain a "pdm-control" file;
 * everything else in the folder becomes data.tar.gz.
 */
#ifndef PMM_PDM_H
#define PMM_PDM_H

/* Pack directory (containing pdm-control) into out.pdm. out may be NULL
 * (defaults to <Package>_<Version>.pdm next to the dir). */
int pdm_pack(const char *dir, const char *out);

/* Install a local .pdm into root dir (~/.pmm/root), record in db (~/.pmm/installed). */
int pdm_install_file(const char *pdmfile);

/* Remove installed package by name (dpkg -r style: delete data files). */
int pdm_remove(const char *name);

/* Print installed packages from the db. */
void pdm_list_installed(void);

/* Print control fields of a .pdm file. */
int pdm_info(const char *pdmfile);

#endif
