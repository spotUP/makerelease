#include "wrap.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int add_line(rp_lines *l, const char *s, size_t n) {
    char **nl = realloc(l->lines, (l->n + 1) * sizeof *nl);
    char *copy = malloc(n + 1);
    if (!nl || !copy) {
        free(nl);
        free(copy);
        return -1;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    l->lines = nl;
    l->lines[l->n++] = copy;
    return 0;
}

rp_lines rp_wrap_words(const char *text, int first_width, int cont_width) {
    rp_lines l = { NULL, 0 };
    char *cur = NULL;
    size_t curlen = 0, curcap = 0;
    int width = first_width;
    const char *p = text;
    for (;;) {
        const char *w;
        size_t wlen, need;
        while (is_ws(*p)) p++;
        if (*p == '\0') break;
        w = p;
        while (*p && !is_ws(*p)) p++;
        wlen = (size_t)(p - w);
        if (curlen > 0 && (int)(curlen + 1 + wlen) > width) {
            if (add_line(&l, cur, curlen) != 0) break;
            curlen = 0;
            width = cont_width;
        }
        need = curlen + (curlen > 0 ? 1 : 0) + wlen + 1;
        if (need > curcap) {
            char *nc = realloc(cur, need * 2);
            if (!nc) break;
            cur = nc;
            curcap = need * 2;
        }
        if (curlen > 0) cur[curlen++] = ' ';
        memcpy(cur + curlen, w, wlen);
        curlen += wlen;
        cur[curlen] = '\0';
    }
    if (curlen > 0 || l.n == 0) {
        if (add_line(&l, cur ? cur : "", curlen) != 0) {
            rp_free_lines(&l);
        }
    }
    free(cur);
    return l;
}

void rp_free_lines(rp_lines *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->lines[i]);
    free(l->lines);
    l->lines = NULL;
    l->n = 0;
}
