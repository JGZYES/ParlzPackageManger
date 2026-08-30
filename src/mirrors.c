/* mirrors.c - apt-style mirror handling
 *
 * mirror.ini / mirror.conf format:
 *   [name]
 *   api = https://mirror.example.com        # replaces git-host API base
 *   download = https://ghfast.example       # gh-proxy style download prefix
 *   registry = https://mirror.example/pmm   # registry mirror ({base}/{pkg}.json)
 *   priority = 1                            # lower tried first (apt-like order)
 */
#include "mirrors.h"
#include "ini.h"
#include "pmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup_or(const char *s, const char *dflt) {
    return strdup(s ? s : dflt);
}

static int cmp_priority(const void *a, const void *b) {
    const Mirror *ma = a, *mb = b;
    return ma->priority - mb->priority;
}

MirrorList *mirrors_load(void) {
    char dir[1024];
    pmm_config_dir(dir, sizeof(dir));
    char *path = pmm_find_config(dir, "mirror");
    if (!path) return calloc(1, sizeof(MirrorList));
    Ini *ini = ini_load(path);
    MirrorList *list = calloc(1, sizeof(MirrorList));
    if (!ini) return list;

    int cap = 0;
    char cursec[512] = "";
    for (IniEntry *e = ini->head; e; e = e->next) {
        if (strcmp(e->section, cursec) != 0) {
            strncpy(cursec, e->section, sizeof(cursec) - 1);
            /* new mirror section begins */
            if (list->count >= cap) {
                cap = cap ? cap * 2 : 8;
                list->items = realloc(list->items, cap * sizeof(Mirror));
            }
            Mirror *m = &list->items[list->count++];
            memset(m, 0, sizeof(*m));
            m->name = xstrdup_or(e->section, "");
            m->priority = 100;
        }
        Mirror *m = list->count ? &list->items[list->count - 1] : NULL;
        if (!m) continue;
        if (strcmp(e->key, "api") == 0) m->api = xstrdup_or(e->value, "");
        else if (strcmp(e->key, "download") == 0) m->download = xstrdup_or(e->value, "");
        else if (strcmp(e->key, "registry") == 0) m->registry = xstrdup_or(e->value, "");
        else if (strcmp(e->key, "priority") == 0) m->priority = atoi(e->value);
    }
    ini_free(ini);
    qsort(list->items, list->count, sizeof(Mirror), cmp_priority);
    return list;
}

void mirrors_free(MirrorList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].api);
        free(list->items[i].download);
        free(list->items[i].registry);
    }
    free(list->items);
    free(list);
}

Mirror *mirrors_active(const MirrorList *list, const char *active_name) {
    if (!active_name || !*active_name) return NULL;
    for (int i = 0; i < list->count; i++)
        if (strcmp(list->items[i].name, active_name) == 0)
            return &list->items[i];
    return NULL;
}

char **mirrors_download_candidates(const MirrorList *list, const char *url, int *out_n) {
    int cap = list->count + 1, n = 0;
    char **cands = malloc(cap * sizeof(char *));
    for (int i = 0; i < list->count && n < cap - 1; i++) {
        const char *dl = list->items[i].download;
        if (!dl || !*dl) continue;
        size_t len = strlen(dl) + strlen(url) + 2;
        cands[n] = malloc(len);
        /* gh-proxy convention: prefix + full original URL */
        if (dl[strlen(dl) - 1] == '=' || dl[strlen(dl) - 1] == '/')
            snprintf(cands[n], len, "%s%s", dl, url);
        else
            snprintf(cands[n], len, "%s/%s", dl, url);
        n++;
    }
    cands[n++] = strdup(url); /* direct last, like apt falling back to origin */
    *out_n = n;
    return cands;
}
