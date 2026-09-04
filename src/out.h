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

#include <stddef.h>

/* Global output flags consulted by the pmm_* helpers.
 *   pmm_no_color  : 1 = never emit ANSI colour (even on a tty). Set by --no-color
 *                   or PMM_NO_COLOR=1.
 *   pmm_log_level : 0 = normal (INFO+SUCCESS+WARN+ERROR prints).
 *                   1 = quiet (-q/--quiet): only WARN and ERROR print.
 *                   2 = verbose (--verbose): INFO/SUCCESS print plus extra debug.
 */
extern int pmm_no_color;
extern int pmm_log_level;

void pmm_error(const char *fmt, ...);
void pmm_success(const char *fmt, ...);
void pmm_info(const char *fmt, ...);
void pmm_warn(const char *fmt, ...);

#endif