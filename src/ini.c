/* ini.c - tiny INI / .conf parser */
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

Ini *ini_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    Ini *ini = calloc(1, sizeof(Ini));
    if (!ini) { fclose(f); return NULL; }
    char line[4096];
    char section[512] = "";
    IniEntry *tail = NULL;
    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            char *close = strchr(s, ']');
            if (close) {
                *close = '\0';
                strncpy(section, s + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
                char *t = trim(section);
                memmove(section, t, strlen(t) + 1);
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) eq = strchr(s, ':');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(s);
        char *value = trim(eq + 1);
        IniEntry *e = calloc(1, sizeof(IniEntry));
        e->section = strdup(*section ? section : "");
        e->key = strdup(key);
        e->value = strdup(value);
        if (tail) tail->next = e; else ini->head = e;
        tail = e;
    }
    fclose(f);
    return ini;
}

void ini_free(Ini *ini) {
    if (!ini) return;
    IniEntry *e = ini->head;
    while (e) {
        IniEntry *n = e->next;
        free(e->section); free(e->key); free(e->value); free(e);
        e = n;
    }
    free(ini);
}

const char *ini_get(const Ini *ini, const char *section, const char *key) {
    if (!ini) return NULL;
    if (!section) section = "";
    for (IniEntry *e = ini->head; e; e = e->next)
        if (strcmp(e->section, section) == 0 && strcmp(e->key, key) == 0)
            return e->value;
    return NULL;
}

const char *ini_find_key_by_value(const Ini *ini, const char *section, const char *value) {
    if (!ini) return NULL;
    if (!section) section = "";
    for (IniEntry *e = ini->head; e; e = e->next)
        if (strcmp(e->section, section) == 0 && strcmp(e->value, value) == 0)
            return e->key;
    return NULL;
}
