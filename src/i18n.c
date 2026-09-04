/* i18n.c - .pjson language-pack (translation) support */
#include "i18n.h"
#include "json.h"
#include "pmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static JsonValue *g_lang = NULL;
static char g_locale[64] = "";

/* Detect locale from LANG / LC_ALL env, else default "zh-CN". */
static void detect_locale(void) {
    const char *lc = getenv("LC_ALL");
    if (!lc || !*lc) lc = getenv("LANG");
    if (!lc || !*lc) lc = "zh-CN";
    /* Normalise: 'en_US.UTF-8' -> 'en-US' */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", lc);
    char *dot = strchr(buf, '.');
    if (dot) *dot = '\0';
    char *underscore = strchr(buf, '_');
    if (underscore) *underscore = '-';
    if (g_locale[0] == '\0') snprintf(g_locale, sizeof(g_locale), "%s", buf);
}

void pmm_lang_set_locale(const char *locale) {
    if (!locale || !*locale) { g_locale[0] = '\0'; return; }
    snprintf(g_locale, sizeof(g_locale), "%s", locale);
}

const char *pmm_lang_locale(void) {
    if (g_locale[0] == '\0') detect_locale();
    return g_locale;
}

int pmm_lang_load(const char *path) {
    JsonValue *root = json_parse_file(path);
    if (!root) return -1;
    if (g_lang) json_free(g_lang);
    g_lang = root;
    return 0;
}

int pmm_lang_load_active(void) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    char path[1400];
    snprintf(path, sizeof(path), "%s/lang/%s.pjson", dir, pmm_lang_locale());
    if (pmm_lang_load(path) == 0) return 0;
    return 1;   /* no pack on disk */
}

const char *pmm_tr(const char *key) {
    if (!g_lang) return key;
    const char *t = json_str(g_lang, key);
    return t ? t : key;
}

const char *pmm_tr_fmt(const char *key, ...) {
    /* pmm_tr returns a pointer into the parsed JSON (or the key). We safe-copy
     * the format string into a static buffer, then vsnprintf + args. */
    static char out[2048];
    const char *fmt = pmm_tr(key);
    /* copy fmt into a local buffer so the static out doesn't alias it */
    char fmtbuf[2048];
    snprintf(fmtbuf, sizeof(fmtbuf), "%s", fmt);
    va_list ap; va_start(ap, key);
    vsnprintf(out, sizeof(out), fmtbuf, ap);
    va_end(ap);
    return out;
}
