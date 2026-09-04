/* i18n.h - .pjson language-pack (translation) support
 *
 * PMM loads a flattened key->value JSON file (".pjson") for the current locale
 * and resolves user-facing strings through pmm_tr()/pmm_tr_fmt(). Keys follow
 * docs/STANDARD.md ("区域.语义", e.g. "msg.installed"). Falls back to the key
 * itself when a translation is absent.
 */
#ifndef PMM_I18N_H
#define PMM_I18N_H

#include <stddef.h>

/* Set the active locale (e.g. "zh-CN"). If non-NULL/empty it overrides the
 * locale auto-detected from LANG/LC_ALL. Copies the value. */
void pmm_lang_set_locale(const char *locale);

/* Current locale name (static buffer, e.g. "zh-CN"). */
const char *pmm_lang_locale(void);

/* Load a .pjson file (flattened key->value object) into the active translation
 * table. Returns 0 on success. On failure the previous table stays. */
int pmm_lang_load(const char *path);

/* Load the active locale's pack from ~/.pmm/lang/<locale>.pjson, if present.
 * Returns 0 if a pack was loaded, 1 if not found (no translation available). */
int pmm_lang_load_active(void);

/* Translate a key to the current locale's text; returns key if unknown/missing.
 * The returned pointer is valid until the next pmm_lang_load*(). */
const char *pmm_tr(const char *key);

/* Translate + format: pmm_tr_fmt("msg.err.not-found", name) -> formatted text.
 * Uses pmm_tr() then printf-style %s/%d replacement. */
const char *pmm_tr_fmt(const char *key, ...);

#endif
