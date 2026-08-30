/* json.h - minimal JSON parser for PMM */
#ifndef PMM_JSON_H
#define PMM_JSON_H

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;
struct JsonValue {
    JsonType type;
    int boolean;
    double number;
    char *string;            /* decoded string (JSON_STRING) */
    JsonValue **items;       /* array/object members (values) */
    char **keys;             /* object member keys */
    int count;
    int capacity;
};

JsonValue *json_parse(const char *text);
JsonValue *json_parse_file(const char *path);
void json_free(JsonValue *v);
/* Object member lookup by key; returns NULL if missing or not an object. */
JsonValue *json_get(const JsonValue *obj, const char *key);
/* Array index; returns NULL if out of range. */
JsonValue *json_at(const JsonValue *arr, int i);
/* Convenience: nested get, e.g. json_path(v, "assets", 0-ish) not supported; use chain. */
const char *json_str(const JsonValue *obj, const char *key); /* NULL if absent */

#endif
