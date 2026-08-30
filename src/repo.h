/* repo.h - git hosting adapters: GitHub / GitLab / Gitea / Forgejo */
#ifndef PMM_REPO_H
#define PMM_REPO_H

#include "pmm.h"

typedef struct {
    char *url;      /* asset download URL */
    char *name;     /* asset file name */
    char *tag;      /* release tag */
} ReleaseAsset;

typedef struct {
    PmmHost host;    /* resolved host (HOST_AUTO is resolved during latest_asset) */
    char *api_url;   /* resolved API endpoint (after mirror rewriting) */
    char *page_url;  /* resolved raw/home endpoint */
    char *origin;    /* scheme://host for full URLs (HOST_AUTO probing) */
    char *repo_norm; /* normalized "owner/repo" */
} RepoContext;

/* Detect host from a repo slug like "owner/repo" or a full URL. */
PmmHost repo_detect_host(const char *repo_or_url);

/* Build context for host + repo. repo may be "owner/repo" or full URL. */
RepoContext *repo_open(PmmHost host, const char *repo, const char *mirror_api_base);

/* Get the download URL of the best asset for the current OS from the latest
 * release. Returns NULL if none found. Caller frees via repo_asset_free. */
ReleaseAsset *repo_latest_asset(RepoContext *ctx, PmmOS os);

void repo_asset_free(ReleaseAsset *a);
void repo_close(RepoContext *ctx);

/* Asset-extension <-> OS matching used by repo_latest_asset (exposed for tests). */
int asset_matches_os(const char *filename, PmmOS os);

#endif
