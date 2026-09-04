/* out.h - colored/structured console output
 *
 * Every user-facing message goes through one of these four helpers so PMM gets
 * a consistent "[PMM]:[LEVEL]" prefix and, on a real terminal, ANSI colour:
 *   pmm_error    -> stderr, red   [PMM]:[ERROR]
 *   pmm_success  -> stdout, green [PMM]:[SUCCESS]
 *   pmm_info     -> stdout, grey  [PMM]:[INFO]
 *   pmm_warn     -> stderr, yellow [PMM]:[WARN]
 *
 * Colour is emitted ONLY when the output stream is a terminal (isatty); when
 * redirecting to a file/pipe the text is printed plain with no escape codes.
 */
#ifndef PMM_OUT_H
#define PMM_OUT_H

void pmm_error(const char *fmt, ...);
void pmm_success(const char *fmt, ...);
void pmm_info(const char *fmt, ...);
void pmm_warn(const char *fmt, ...);

#endif