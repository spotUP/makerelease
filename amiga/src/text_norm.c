#include "text_norm.h"

#include <string.h>

size_t rp_text_normalize(const char *in, char *out, size_t cap) {
    size_t n = 0, text_end = 0;
    int have_text = 0;
    const char *p = in ? in : "";
    if (cap == 0) return 0;
    while (*p && n + 1 < cap) {
        const char *e = p;
        size_t len, i;
        while (*e && *e != '\n' && *e != '\r') e++;
        len = (size_t)(e - p);
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
        if (len > 0 || have_text) {
            for (i = 0; i < len && n + 1 < cap; i++) out[n++] = p[i];
            if (len > 0) { have_text = 1; text_end = n; }
            if (n + 1 < cap) out[n++] = '\n';
        }
        if (*e == '\r' && e[1] == '\n') e++;
        p = *e ? e + 1 : e;
    }
    /* text_end is the end of the last text line: drops trailing blank lines and the final LF */
    n = text_end;
    out[n] = '\0';
    return n;
}
