/* ini.h - tiny INI / .conf parser for PMM.
 * Format:
 *   [section]
 *   key = value     (# or ; comment)
 * Also accepts key = value lines without a section (section = "").
 */
#ifndef PMM_INI_H
#define PMM_INI_H

typedef struct IniEntry {
    char *section;
    char *key;
    char *value;
    struct IniEntry *next;
} IniEntry;

typedef struct {
    IniEntry *head;
} Ini;

Ini *ini_load(const char *path);
void ini_free(Ini *ini);
/* Lookup value; section may be NULL or "" for top-level. Returns NULL if absent. */
const char *ini_get(const Ini *ini, const char *section, const char *key);
/* First key in given section whose value matches; returns key name or NULL. */
const char *ini_find_key_by_value(const Ini *ini, const char *section, const char *value);

#endif
