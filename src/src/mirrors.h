/* mirrors.h - apt-style mirror list with priorities and download fallback */
#ifndef PMM_MIRRORS_H
#define PMM_MIRRORS_H

typedef struct Mirror {
    char *name;
    char *api;        /* API base (rewrites github/gitlab/gitea API endpoints) */
    char *download;   /* download prefix (gh-proxy style: prefix + original URL) */
    char *registry;   /* apt-style registry mirror: serves {registry}/{pkg}.json */
    int priority;     /* lower = tried first (default 100) */
} Mirror;

typedef struct {
    Mirror *items;
    int count;
} MirrorList;

/* Load mirrors from ~/.pmm/mirror.ini / mirror.conf, sorted by priority. */
MirrorList *mirrors_load(void);
void mirrors_free(MirrorList *list);
/* If no mirror config exists, write a default mirror.ini (sz + main). */
void pmm_ensure_default_mirror(const char *dir);
/* Mirror whose name matches the active selection in pmm config, or NULL. */
Mirror *mirrors_active(const MirrorList *list, const char *active_name);

/* Build candidate download URLs for `url` (caller frees each string and array).
 * Order: prefix-style mirrors by priority, then the original URL. */
char **mirrors_download_candidates(const MirrorList *list, const char *url, int *out_n);

#endif
