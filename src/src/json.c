/* json.c - minimal recursive-descent JSON parser (no external deps) */
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { const char *p; const char *end; } Parser;

static JsonValue *parse_value(Parser *ps);

static JsonValue *jnew(JsonType t) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    if (!v) { fprintf(stderr, "pmm: out of memory\n"); exit(1); }
    v->type = t;
    return v;
}

void json_free(JsonValue *v) {
    if (!v) return;
    free(v->string);
    for (int i = 0; i < v->count; i++) {
        if (v->keys) free(v->keys[i]);
        json_free(v->items[i]);
    }
    free(v->keys);
    free(v->items);
    free(v);
}

static void jadd(JsonValue *v, char *key, JsonValue *item) {
    if (v->count >= v->capacity) {
        v->capacity = v->capacity ? v->capacity * 2 : 8;
        v->items = realloc(v->items, v->capacity * sizeof(JsonValue *));
        if (v->type == JSON_OBJECT)
            v->keys = realloc(v->keys, v->capacity * sizeof(char *));
        if (!v->items || (v->type == JSON_OBJECT && !v->keys)) {
            fprintf(stderr, "pmm: out of memory\n"); exit(1);
        }
    }
    if (v->type == JSON_OBJECT) v->keys[v->count] = key;
    v->items[v->count++] = item;
}

static void skip_ws(Parser *ps) {
    while (ps->p < ps->end && isspace((unsigned char)*ps->p)) ps->p++;
}

static void fail(const char *msg) {
    fprintf(stderr, "pmm: JSON parse error: %s\n", msg);
    exit(1);
}

static char *parse_string_raw(Parser *ps) {
    if (ps->p >= ps->end || *ps->p != '"') fail("expected string");
    ps->p++;
    size_t cap = 16, len = 0;
    char *buf = malloc(cap);
    if (!buf) fail("out of memory");
    while (ps->p < ps->end && *ps->p != '"') {
        char c = *ps->p++;
        char out[4]; int n = 1;
        if (c == '\\') {
            if (ps->p >= ps->end) fail("bad escape");
            char e = *ps->p++;
            switch (e) {
            case '"': out[0] = '"'; break;
            case '\\': out[0] = '\\'; break;
            case '/': out[0] = '/'; break;
            case 'b': out[0] = '\b'; break;
            case 'f': out[0] = '\f'; break;
            case 'n': out[0] = '\n'; break;
            case 'r': out[0] = '\r'; break;
            case 't': out[0] = '\t'; break;
            case 'u': {
                if (ps->end - ps->p < 4) fail("bad \\u escape");
                unsigned cp = 0;
                for (int i = 0; i < 4; i++) {
                    char h = *ps->p++;
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                    else fail("bad \\u escape");
                }
                /* UTF-8 encode (surrogate pairs not combined: good enough for names/URLs) */
                if (cp < 0x80) { out[0] = (char)cp; n = 1; }
                else if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
                else { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
                break;
            }
            default: fail("bad escape");
            }
        } else {
            out[0] = c;
        }
        if (len + (size_t)n + 1 > cap) {
            cap *= 2;
            buf = realloc(buf, cap);
            if (!buf) fail("out of memory");
        }
        memcpy(buf + len, out, (size_t)n);
        len += (size_t)n;
    }
    if (ps->p >= ps->end) fail("unterminated string");
    ps->p++; /* closing quote */
    buf[len] = '\0';
    return buf;
}

static JsonValue *parse_number(Parser *ps) {
    char *endp;
    double d = strtod(ps->p, &endp);
    if (endp == ps->p) fail("bad number");
    ps->p = endp;
    JsonValue *v = jnew(JSON_NUMBER);
    v->number = d;
    return v;
}

static JsonValue *parse_value(Parser *ps) {
    skip_ws(ps);
    if (ps->p >= ps->end) fail("unexpected end");
    char c = *ps->p;
    if (c == '{') {
        ps->p++;
        JsonValue *v = jnew(JSON_OBJECT);
        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == '}') { ps->p++; return v; }
        for (;;) {
            skip_ws(ps);
            char *key = parse_string_raw(ps);
            skip_ws(ps);
            if (ps->p >= ps->end || *ps->p != ':') fail("expected ':'");
            ps->p++;
            JsonValue *item = parse_value(ps);
            jadd(v, key, item);
            skip_ws(ps);
            if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
            if (ps->p < ps->end && *ps->p == '}') { ps->p++; return v; }
            fail("expected ',' or '}'");
        }
    } else if (c == '[') {
        ps->p++;
        JsonValue *v = jnew(JSON_ARRAY);
        skip_ws(ps);
        if (ps->p < ps->end && *ps->p == ']') { ps->p++; return v; }
        for (;;) {
            JsonValue *item = parse_value(ps);
            jadd(v, NULL, item);
            skip_ws(ps);
            if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
            if (ps->p < ps->end && *ps->p == ']') { ps->p++; return v; }
            fail("expected ',' or ']'");
        }
    } else if (c == '"') {
        JsonValue *v = jnew(JSON_STRING);
        v->string = parse_string_raw(ps);
        return v;
    } else if (c == 't') {
        if (ps->end - ps->p < 4 || strncmp(ps->p, "true", 4)) fail("bad literal");
        ps->p += 4;
        JsonValue *v = jnew(JSON_BOOL); v->boolean = 1; return v;
    } else if (c == 'f') {
        if (ps->end - ps->p < 5 || strncmp(ps->p, "false", 5)) fail("bad literal");
        ps->p += 5;
        JsonValue *v = jnew(JSON_BOOL); return v;
    } else if (c == 'n') {
        if (ps->end - ps->p < 4 || strncmp(ps->p, "null", 4)) fail("bad literal");
        ps->p += 4;
        return jnew(JSON_NULL);
    } else {
        return parse_number(ps);
    }
}

JsonValue *json_parse(const char *text) {
    Parser ps = { text, text + strlen(text) };
    JsonValue *v = parse_value(&ps);
    return v;
}

char *read_whole_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = (long)rd;
    return buf;
}

JsonValue *json_parse_file(const char *path) {
    char *text = read_whole_file(path, NULL);
    if (!text) return NULL;
    JsonValue *v = json_parse(text);
    free(text);
    return v;
}

JsonValue *json_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (int i = 0; i < obj->count; i++)
        if (strcmp(obj->keys[i], key) == 0) return obj->items[i];
    return NULL;
}

JsonValue *json_at(const JsonValue *arr, int i) {
    if (!arr || arr->type != JSON_ARRAY || i < 0 || i >= arr->count) return NULL;
    return arr->items[i];
}

const char *json_str(const JsonValue *obj, const char *key) {
    JsonValue *v = json_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->string : NULL;
}
