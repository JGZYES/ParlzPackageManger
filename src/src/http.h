/* http.h - HTTP(S) via system curl (curl is preinstalled on Win10+/macOS/most Linux) */
#ifndef PMM_HTTP_H
#define PMM_HTTP_H

#include <stddef.h>

/* GET a URL, return malloc'd body or NULL on failure. Sets *status (may be NULL). */
char *http_get(const char *url, int *status);

/* Download URL to file path. Returns 0 on success. */
int http_download(const char *url, const char *out_path);

#endif
