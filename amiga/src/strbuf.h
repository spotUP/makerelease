#ifndef RP_STRBUF_H
#define RP_STRBUF_H

#include <stdlib.h>
#include <string.h>

/* Growing NUL-terminated byte buffer shared by the NFO renderers. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static inline int sb_putn(strbuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 1024;
        while (cap < b->len + n + 1) cap *= 2;
        {
            char *p = realloc(b->data, cap);
            if (!p) return -1;
            b->data = p;
            b->cap = cap;
        }
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static inline int sb_puts(strbuf *b, const char *s) {
    return sb_putn(b, s, strlen(s));
}

#endif
