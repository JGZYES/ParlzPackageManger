/* repo.c - git hosting adapters: GitHub / GitLab / Gitea / Forgejo
 *
 * API endpoints used (latest release):
 *   GitHub:        {api}/repos/{repo}/releases/latest
 *   GitLab:        {api}/projects/{url-encoded repo}/releases/permalink/latest
 *   Gitea/Forgejo: {api}/api/v1/repos/{repo}/releases/latest
 *
 * Asset selection by OS (first match wins, in priority order):
 *   windows: .exe .msi .zip .7z
 *   linux:   .deb .rpm .appimage .tar.gz .tgz .tar.xz
 *   macos:   .dmg .pkg .app.zip .tar.gz
 */
#include "repo.h"
#include "http.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *strdup_or(const char *s, const char *dflt) {
    return strdup(s ? s : dflt);
}

static char *url_encode(const char *s) {
    size_t cap = strlen(s) * 3 + 1;
    char *out = malloc(cap);
    size_t o = 0;
    for (const char *p = s; *p && o + 4 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else if (c == '/') {
            out[o++] = '%'; out[o++] = '2'; out[o++] = 'F';
        } else {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%%%02X", c);
            out[o++] = tmp[0]; out[o++] = tmp[1]; out[o++] = tmp[2];
        }
    }
    out[o] = '\0';
    return out;
}

/* Normalize "https://host/owner/repo(.git)" or "owner/repo" into "owner/repo". */
static char *normalize_repo(const char *in) {
    const char *s = in;
    int is_url = 0;
    if (strncmp(s, "https://", 8) == 0) { s += 8; is_url = 1; }
    else if (strncmp(s, "http://", 7) == 0) { s += 7; is_url = 1; }
    /* skip hostname only for full URLs */
    if (is_url) {
        const char *slash = strchr(s, '/');
        if (slash && slash != s) s = slash + 1;
    }
    size_t len = strlen(s);
    if (len > 4 && strcmp(s + len - 4, ".git") == 0) len -= 4;
    /* strip trailing slashes */
    while (len > 0 && s[len - 1] == '/') len--;
    char *out = malloc(len + 1);
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

PmmHost repo_detect_host(const char *repo_or_url) {
    if (strstr(repo_or_url, "github.com")) return HOST_GITHUB;
    if (strstr(repo_or_url, "gitlab.")) return HOST_GITLAB;
    if (strstr(repo_or_url, "gitea.")) return HOST_GITEA;
    if (strstr(repo_or_url, "forgejo.")) return HOST_FORGEJO;
    /* full URL with unknown host: try to infer from path; default gitea-compatible */
    if (strncmp(repo_or_url, "http", 4) == 0) return HOST_GITEA;
    return HOST_GITHUB; /* bare slug defaults to github */
}

/* Extract "scheme://host[:port]" from a URL; empty string if none. */
static void extract_origin(const char *repo, char *origin, size_t size) {
    const char *s = repo;
    origin[0] = '\0';
    if (strncmp(s, "https://", 8) == 0) s += 8;
    else if (strncmp(s, "http://", 7) == 0) { s += 7; }
    else return; /* bare slug: no origin */
    s -= (strncmp(repo, "https://", 8) == 0) ? 8 : 7;
    const char *slash = strchr(s + 8, '/');
    if (!slash) slash = s + strlen(s);
    size_t n = (size_t)(slash - s);
    if (n >= size) n = size - 1;
    memcpy(origin, s, n);
    origin[n] = '\0';
}

RepoContext *repo_open(PmmHost host, const char *repo, const char *mirror_api_base) {
    RepoContext *ctx = calloc(1, sizeof(RepoContext));
    if (!ctx) return NULL;
    ctx->host = host;
    char *norm = normalize_repo(repo);
    char api[1024];
    ctx->repo_norm = strdup(norm);

    char origin[512];
    extract_origin(repo, origin, sizeof(origin));
    ctx->origin = strdup(origin);

    const char *base = NULL;
    if (mirror_api_base && *mirror_api_base) {
        base = mirror_api_base; /* mirror overrides host entirely */
    } else {
        switch (host) {
        case HOST_GITHUB: base = "https://api.github.com"; break;
        case HOST_GITLAB: base = "https://gitlab.com"; break;
        default: base = NULL; break; /* gitea/forgejo/auto need a full URL */
        }
    }

    if (host == HOST_AUTO) {
        /* Generic git URL: api_url is built per-candidate in repo_latest_asset */
        ctx->page_url = strdup_or(origin[0] ? origin : "", "");
        ctx->api_url = NULL;
        free(norm);
        return ctx;
    }

    if (!base) {
        /* Gitea/Forgejo require a full URL in the repo argument, e.g.
         * https://git.example.com/owner/repo  or configured in mirror file. */
        if (!origin[0])
            strncpy(origin, "http://localhost:3000", sizeof(origin) - 1);
        snprintf(api, sizeof(api), "%s/api/v1/repos/%s", origin, norm);
        ctx->page_url = strdup_or(origin, "");
    } else {
        switch (host) {
        case HOST_GITHUB:
            snprintf(api, sizeof(api), "%s/repos/%s", base, norm);
            break;
        case HOST_GITLAB: {
            char *enc = url_encode(norm);
            snprintf(api, sizeof(api), "%s/projects/%s", base, enc);
            free(enc);
            break;
        }
        default: /* gitea/forgejo via mirror base */
            snprintf(api, sizeof(api), "%s/api/v1/repos/%s", base, norm);
            break;
        }
        ctx->page_url = strdup(base);
    }
    ctx->api_url = strdup(api);
    free(norm);
    return ctx;
}

void repo_close(RepoContext *ctx) {
    if (!ctx) return;
    free(ctx->api_url);
    free(ctx->page_url);
    free(ctx->origin);
    free(ctx->repo_norm);
    free(ctx);
}

static int has_ext(const char *name, const char *ext) {
    size_t nl = strlen(name), el = strlen(ext);
    if (nl < el) return 0;
    const char *tail = name + nl - el;
    if (strcasecmp(tail, ext) != 0) return 0;
    /* ".tar.gz" style: for ".gz" only match after ".tar"? keep simple: exact suffix */
    return 1;
}

static int asset_is_junk(const char *name);
int asset_matches_os(const char *filename, PmmOS os) {
    static const char *win_exts[] = { ".exe", ".msi", ".zip", ".7z", NULL };
    static const char *linux_exts[] = { ".deb", ".rpm", ".appimage", ".tar.gz", ".tgz", ".tar.xz", ".tar.bz2", ".pkg.tar.zst", NULL };
    static const char *mac_exts[] = { ".dmg", ".pkg", ".app.zip", ".tar.gz", ".tgz", NULL };
    const char **exts = NULL;
    switch (os) {
    case OS_WINDOWS: exts = win_exts; break;
    case OS_LINUX:   exts = linux_exts; break;
    case OS_MACOS:   exts = mac_exts; break;
    default: return 0;
    }
    for (int i = 0; exts[i]; i++)
        if (has_ext(filename, exts[i])) return 1;
    /* A bare (no-dot) binary counts on Linux/macOS — e.g. a release asset named
     * "pmm" that is an ELF. Skip obvious junk like README/LICENCE/checksums. */
    if ((os == OS_LINUX || os == OS_MACOS) && strchr(filename, '.') == NULL
        && !asset_is_junk(filename))
        return 1;
    return 0;
}

/* Reject common junk assets like sources, checksums, sigs. */
static int asset_is_junk(const char *name) {
    static const char *junk[] = { ".sha256", ".sha512", ".sig", ".asc", ".sbom",
                                  ".json", ".txt", ".yml", ".yaml", ".xml", ".sb", ".pem", NULL };
    for (int i = 0; junk[i]; i++)
        if (has_ext(name, junk[i])) return 1;
    if (strstr(name, "checksum") || strstr(name, "source") || strstr(name, "-src")) return 1;
    return 0;
}

static ReleaseAsset *asset_from_json(const JsonValue *a, const char *tag) {
    ReleaseAsset *ra = calloc(1, sizeof(ReleaseAsset));
    const char *name = json_str(a, "name");
    const char *url = json_str(a, "browser_download_url");
    if (!url) url = json_str(a, "direct_asset_url");
    ra->name = strdup_or(name, "asset");
    ra->url = strdup_or(url, "");
    ra->tag = strdup_or(tag, "");
    return ra;
}

static ReleaseAsset *pick_from_assets_array(const JsonValue *assets, const char *tag, PmmOS os) {
    for (int i = 0; i < assets->count; i++) {
        JsonValue *a = json_at(assets, i);
        if (!a) continue;
        const char *name = json_str(a, "name");
        if (!name || asset_is_junk(name)) continue;
        if (!asset_matches_os(name, os)) continue;
        return asset_from_json(a, tag);
    }
    return NULL;
}

static ReleaseAsset *fetch_latest(const char *url, PmmHost host, PmmOS os) {
    int status = 0;
    char *body = http_get(url, &status);
    if (!body || (status != 200 && status != 0)) {
        free(body);
        return NULL;
    }
    JsonValue *root = json_parse(body);
    free(body);
    if (!root) return NULL;

    const char *tag = json_str(root, "tag_name");
    if (!tag) tag = json_str(root, "tag");

    ReleaseAsset *found = NULL;

    /* GitHub: assets is at root; Gitea: same; GitLab: assets.links[] */
    JsonValue *assets = json_get(root, "assets");
    if (host == HOST_GITLAB && assets) {
        JsonValue *links = json_get(assets, "links");
        if (links) {
            for (int i = 0; i < links->count && !found; i++) {
                JsonValue *a = json_at(links, i);
                const char *name = json_str(a, "name");
                if (!name) name = json_str(a, "direct_asset_url");
                if (!name || asset_is_junk(name) || !asset_matches_os(name, os)) continue;
                found = calloc(1, sizeof(ReleaseAsset));
                found->name = strdup(name);
                found->url = strdup_or(json_str(a, "direct_asset_url"), json_str(a, "url") ? json_str(a, "url") : "");
                found->tag = strdup_or(tag, "");
            }
        }
    } else if (assets) {
        found = pick_from_assets_array(assets, tag, os);
    }

    json_free(root);
    if (found && (!found->url || !*found->url)) {
        repo_asset_free(found);
        return NULL;
    }
    return found;
}

ReleaseAsset *repo_latest_asset(RepoContext *ctx, PmmOS os) {
    char url[2048];

    if (ctx->host == HOST_AUTO) {
        /* Generic git repo: probe known API shapes on the given origin. */
        const char *origin = (ctx->origin && *ctx->origin) ? ctx->origin : NULL;
        const char *norm = ctx->repo_norm;
        ReleaseAsset *a = NULL;
        if (origin) {
            snprintf(url, sizeof(url), "%s/api/v1/repos/%s/releases/latest", origin, norm);
            if ((a = fetch_latest(url, HOST_GITEA, os))) { ctx->host = HOST_GITEA; return a; }
            char *enc = url_encode(norm);
            snprintf(url, sizeof(url), "%s/api/v4/projects/%s/releases/permalink/latest", origin, enc);
            free(enc);
            if ((a = fetch_latest(url, HOST_GITLAB, os))) { ctx->host = HOST_GITLAB; return a; }
            if (strstr(origin, "github.com")) {
                snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", norm);
                if ((a = fetch_latest(url, HOST_GITHUB, os))) { ctx->host = HOST_GITHUB; return a; }
            }
        } else {
            snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", norm);
            if ((a = fetch_latest(url, HOST_GITHUB, os))) { ctx->host = HOST_GITHUB; return a; }
            snprintf(url, sizeof(url), "https://gitlab.com/api/v4/projects/%s/releases/permalink/latest", url_encode(norm));
            if ((a = fetch_latest(url, HOST_GITLAB, os))) { ctx->host = HOST_GITLAB; return a; }
        }
        return NULL;
    }

    switch (ctx->host) {
    case HOST_GITLAB:
        snprintf(url, sizeof(url), "%s/releases/permalink/latest", ctx->api_url);
        break;
    default:
        snprintf(url, sizeof(url), "%s/releases/latest", ctx->api_url);
        break;
    }
    return fetch_latest(url, ctx->host, os);
}

void repo_asset_free(ReleaseAsset *a) {
    if (!a) return;
    free(a->url); free(a->name); free(a->tag);
    free(a);
}
