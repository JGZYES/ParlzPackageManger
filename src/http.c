/* http.c - HTTP(S) via the system curl executable */
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define PMM_POPEN _popen
#define PMM_PCLOSE _pclose
#define PMM_POPEN_READ(cmd) _popen(cmd, "r")      /* text: header/body capture */
#define PMM_PCLOSE_READ(p) _pclose(p)
#define PMM_POPEN_READ_X(cmd) _popen(cmd, "rb")   /* binary: download streams */
#define PMM_PCLOSE_READ_X(p) _pclose(p)
#define STDERR_FD _fileno(stderr)
#define IS_TTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define PMM_POPEN popen
#define PMM_PCLOSE pclose
#define PMM_POPEN_READ(cmd) popen(cmd, "r")
#define PMM_PCLOSE_READ(p) pclose(p)
#define PMM_POPEN_READ_X(cmd) popen(cmd, "r")
#define PMM_PCLOSE_READ_X(p) pclose(p)
#define STDERR_FD fileno(stderr)
#define IS_TTY(fd) isatty(fd)
#endif

/* Read all output of `cmd` into a malloc'd buffer. */
static char *run_capture(const char *cmd, size_t *out_len) {
    FILE *f = PMM_POPEN(cmd, "rb");
    if (!f) return NULL;
    size_t cap = 64 * 1024, len = 0;
    char *buf = malloc(cap);
    if (!buf) { PMM_PCLOSE(f); return NULL; }
    for (;;) {
        if (len + 4096 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); PMM_PCLOSE(f); return NULL; }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 4096, f);
        len += n;
        if (n == 0) break;
    }
    int rc = PMM_PCLOSE(f);
    if (getenv("PMM_DEBUG")) fprintf(stderr, "[http] pclose rc=%d len=%zu\n", rc, len);
    if (rc != 0 && len == 0) { free(buf); return NULL; }
    if (out_len) *out_len = len;
    buf[len] = '\0';
    return buf;
}

/* Run `curl <args>` with the URL appended; return captured output. */
static char *curl_capture(const char *args, const char *url, size_t *out_len) {
    size_t cmdlen = strlen(args) + strlen(url) * 3 + 64;
    char *cmd = malloc(cmdlen);
    if (!cmd) return NULL;
    /* wrap URL in double quotes, escaping embedded quotes */
    snprintf(cmd, cmdlen, "curl %s \"", args);
    char *p = cmd + strlen(cmd);
    for (const char *u = url; *u && (size_t)(p - cmd) < cmdlen - 4; u++) {
        if (*u == '"') *p++ = '\\';
        if (*u == '$') *p++ = '\\';
        *p++ = *u;
    }
    *p++ = '"';
    *p = '\0';
    if (getenv("PMM_DEBUG")) fprintf(stderr, "[http] cmd: %s\n", cmd);
    char *res = run_capture(cmd, out_len);
    free(cmd);
    return res;
}

char *http_get(const char *url, int *status) {
    /* The status marker has NO backslash/newline, so it's immune to shell
     * interpretation differences:  body + "__PMM_HTTP_<code>" */
    size_t len = 0;
    if (getenv("PMM_DEBUG")) fprintf(stderr, "[http] GET %s\n", url);
    char *body = curl_capture("-sSL --max-time 60 -w __PMM_HTTP_%{http_code}", url, &len);
    if (getenv("PMM_DEBUG")) fprintf(stderr, "[http] captured len=%zu head=%.60s\n",
                                     body ? len : 0, body ? body : "(null)");
    if (!body) return NULL;
    /* find the LAST marker occurrence, take the digits after it */
    const char *mk = "__PMM_HTTP_";
    size_t mkl = strlen(mk);
    char *last = NULL;
    for (char *p = body; (p = strstr(p, mk)) != NULL; p += mkl) last = p;
    if (last) {
        int code = atoi(last + mkl);
        *last = '\0'; /* strip the marker from the body */
        if (code <= 0) { /* 000 = failed transfer, not success */
            if (status) *status = -1;
            free(body);
            return NULL;
        }
        if (status) *status = code;
        if (getenv("PMM_DEBUG")) fprintf(stderr, "[http] status=%d\n", code);
    } else {
        /* no marker (some curl/quirk): keep body, status unknown -> later logic
         * in install accepts non-404 bodies */
        if (status) *status = -1;
    }
    return body;
}

/* ---------- python/tqdm-style download progress ---------- */

#ifdef _WIN32
#include <windows.h>
static unsigned long long now_ms(void) { return (unsigned long long)GetTickCount64(); }
#else
#include <sys/time.h>
static unsigned long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)tv.tv_usec / 1000ULL;
}
#endif

/* human size: B / KB / MB / GB with 1 decimal */
static void hs(unsigned long long v, char *out, size_t n) {
    const char *u[] = { "B", "KB", "MB", "GB", "TB" };
    int i = 0; double d = (double)v;
    while (d >= 1024.0 && i < 4) { d /= 1024.0; i++; }
    if (i == 0) snprintf(out, n, "%llu B", v);
    else snprintf(out, n, "%.1f%s", d, u[i]);
}

static void hms(double sec, char *out, size_t n) {
    int s = (int)sec;
    if (s >= 3600) snprintf(out, n, "%02d:%02d:%02d", s / 3600, (s % 3600) / 60, s % 60);
    else snprintf(out, n, "%02d:%02d", s / 60, s % 60);
}

/* python-style bar, ASCII-safe (no unicode block glyphs, so it can't mojibake
 * on GBK/936 or other non-UTF-8 Windows console codepages):
 *   93%|##############--------| 36.1MB/38.8MB [01:30<00:05, 560.7KB/s] */
static void render_progress(unsigned long long fetched, unsigned long long total,
                            double elapsed, double speed, int done) {
    int w = 28;
    char line[240];
    if (total == 0) { /* unknown total */
        char f[32], sp[32], et[32];
        hs(fetched, f, sizeof(f));
        hs(speed > 0 ? speed : 0, sp, sizeof(sp));
        hms(elapsed, et, sizeof(et));
        snprintf(line, sizeof(line), "\r  %s [%s, %s/s]", f, et, sp);
    } else {
        double pct = (double)fetched / (double)total * 100.0;
        int fill = (int)(pct / 100.0 * w);
        char bar[32];
        for (int i = 0; i < w; i++) bar[i] = (i < fill) ? '#' : ' ';
        bar[w] = '\0';
        char f[32], t[32], sp[32], e[32], eta[32];
        hs(fetched, f, sizeof(f));
        hs(total, t, sizeof(t));
        hs(speed > 0 ? speed : 0, sp, sizeof(sp));
        hms(elapsed, e, sizeof(e));
        double rem = speed > 0 ? ((double)total - (double)fetched) / speed : 0;
        hms(rem, eta, sizeof(eta));
        snprintf(line, sizeof(line), "\r%3.0f%%|%s| %s/%s [%s<%s, %s/s]",
                 pct >= 100.0 ? 100.0 : pct, bar, f, t, e, eta, sp);
    }
    /* pad to a fixed width that ALWAYS fits in a normal terminal (~72 cols) so
     * the frame overwrites in place; ≥200 spaces would wrap on narrow consoles
     * and that is what makes it look like the bar is spamming many lines. */
    int L = (int)strlen(line);
    while (L < 72) line[L++] = ' ';
    line[L] = '\0';
    fprintf(stderr, "%s", line);
    if (done) fputc('\n', stderr);
    fflush(stderr);
}

/* fetch Content-Length (bytes) via HEAD; returns 0 if unknown */
static unsigned long long remote_size(const char *url) {
    unsigned long long sz = 0;
    char cmd[2100];
    snprintf(cmd, sizeof(cmd), "curl -sIL --max-time 30 \"%s\" 2>nul", url);
    FILE *f = PMM_POPEN_READ(cmd);
    if (f) {
        char line[1024];
        char low[1024];
        while (fgets(line, sizeof(line), f)) {
            strncpy(low, line, sizeof(low) - 1); low[sizeof(low) - 1] = '\0';
            for (char *p = low; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            char *v = strstr(low, "content-length:");
            if (v) { sz = (unsigned long long)strtoull(v + 15, NULL, 10); break; }
        }
        PMM_PCLOSE_READ(f);
    }
    return sz;
}

int http_download(const char *url, const char *out_path) {
    int tty = IS_TTY(STDERR_FD);
    if (getenv("PMM_FORCE_PROGRESS")) tty = 1; /* debug/testing override */

    if (!tty) {
        /* background/redirect: silent, simple */
        size_t cmdlen = strlen(url) * 3 + strlen(out_path) * 3 + 160;
        char *cmd = malloc(cmdlen);
        if (!cmd) return -1;
        snprintf(cmd, cmdlen, "curl -L --fail --retry 3 --max-time 3600 -S --silent -o \"%s\" \"%s\"", out_path, url);
        int rc = system(cmd);
        free(cmd);
        return (rc == 0) ? 0 : -1;
    }

    /* terminal: stream via popen and paint a python-style bar */
    unsigned long long total = remote_size(url);
    char cmd[2100];
    snprintf(cmd, sizeof(cmd), "curl -sL --fail --retry 3 --max-time 3600 -o - \"%s\"", url);
    FILE *pf = PMM_POPEN_READ_X(cmd);   /* binary read */
    if (!pf) return -1;
    FILE *outf = fopen(out_path, "wb");
    if (!outf) { PMM_PCLOSE_READ_X(pf); return -1; }

    char buf[65536];
    size_t r;
    unsigned long long fetched = 0, last = 0, last_n = now_ms();
    double speed = 0, el = 0;
    double start = (double)now_ms();
    while ((r = fread(buf, 1, sizeof(buf), pf)) > 0) {
        fwrite(buf, 1, r, outf);
        fetched += r;
        unsigned long long n = now_ms();
        double dt = (n - last_n) / 1000.0;
        if (dt >= 0.25) {
            double inst = (fetched - last) / dt;
            speed = (speed <= 0) ? inst : (speed * 0.7 + inst * 0.3); /* EMA smooth */
            last = fetched; last_n = n;
        }
        el = ((double)n - start) / 1000.0;
        render_progress(fetched, total, el, speed, 0);
    }
    fclose(outf);
    int rc = PMM_PCLOSE_READ_X(pf);
    if (rc != 0) { remove(out_path); return -1; }
    render_progress(fetched, total, el, speed, 1);
    return 0;
}
