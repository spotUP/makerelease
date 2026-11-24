#include "diz_count.h"

#include <stdio.h>
#include <string.h>

/* Length of s without leading and trailing blanks; *first is the index of its first kept byte. */
static size_t trimmed(const char *s, size_t *first) {
    size_t a = 0, z = strlen(s);
    while (a < z && (s[a] == ' ' || s[a] == '\t')) a++;
    while (z > a && (s[z - 1] == ' ' || s[z - 1] == '\t')) z--;
    *first = a;
    return z - a;
}

void rp_diz_line_counter(size_t width, const char *line, const char *name, const char *place, char *out, size_t cap) {
    size_t a, n = trimmed(line, &a), na, nn, pa, np;
    if (cap == 0) return;
    if (n == 0) {
        nn = trimmed(name, &na);
        np = trimmed(place, &pa);
        /* "<name> at <place>" needs both, like dizLineSourceText */
        n = (nn > 0 && np > 0) ? nn + 4 + np : nn;
    }
    if (n > width) snprintf(out, cap, "Release line: %lu characters, the diz fits %lu; the rest is cut off.", (unsigned long)n, (unsigned long)width);
    else snprintf(out, cap, "Release line: %lu/%lu characters.", (unsigned long)n, (unsigned long)width);
}
