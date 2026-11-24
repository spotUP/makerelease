#ifndef RP_TEXT_NORM_H
#define RP_TEXT_NORM_H

#include <stddef.h>

/* The multi-line Notes and News fields come out of UP-TextArea as LF text. This makes it NFO-ready:
 * CRLF and lone CR become LF, trailing blanks on each line are dropped, leading and trailing blank
 * lines are removed (inner blank lines stay). Writes at most cap-1 bytes plus NUL; returns the length. */
size_t rp_text_normalize(const char *in, char *out, size_t cap);

#endif
