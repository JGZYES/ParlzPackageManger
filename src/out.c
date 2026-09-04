/* out.c - colored/structured console output implementation */
#include "out.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define IS_TTY_FD(fd) _isatty(fd)
#else
#include <unistd.h>
#define IS_TTY_FD(fd) isatty(fd)
#endif

/* Emit a message with an optional ANSI colour and a [PMM]:[LEVEL] prefix.
 * fd_stream: 1 for stdout (info/success), 2 for stderr (error/warn).
 * colour is the ANSI SGR number (0 = no colour), OR "" style. We always write
 * the plain "[PMM]:[LEVEL]" prefix; colour only wraps it when the tty test
 * passes. */
static void pmmsg(FILE *stream, int fd, const char *level, int wrap_color, const char *fmt, va_list ap) {
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    /* Trim any trailing newline(s) from the formatted message — many source
     * calls end their format with "\n", and we add exactly one below. Without
     * this, "[PMM]:[INFO]msg\n" would print a blank line after every message. */
    size_t len = strlen(buf);
    while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';

    int tty = IS_TTY_FD(fd);
    if (tty && wrap_color)
        fprintf(stream, "\033[%dm[PMM]:[%s]%s\033[0m\n", wrap_color, level, buf);
    else
        fprintf(stream, "[PMM]:[%s]%s\n", level, buf);
}

void pmm_error(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    pmmsg(stderr, 2, "ERROR", 31, fmt, ap);   /* red */
    va_end(ap);
}

void pmm_success(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    pmmsg(stdout, 1, "SUCCESS", 32, fmt, ap); /* green */
    va_end(ap);
}

void pmm_info(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    pmmsg(stdout, 1, "INFO", 2, fmt, ap);     /* grey (SGR 2 = dim) */
    va_end(ap);
}

void pmm_warn(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    pmmsg(stderr, 2, "WARN", 33, fmt, ap);    /* yellow */
    va_end(ap);
}
