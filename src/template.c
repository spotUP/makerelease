#include "template.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *const SCALARS[] = {
    "group", "url", "code", "filename", "format", "release_name",
    "platform", "date", "release_number", "description", "creators",
};
static const size_t NSCALARS = sizeof(SCALARS) / sizeof(SCALARS[0]);
static const char *const BLOCKS[] = { "notes", "news", "members", "credits" };
static const size_t NBLOCKS = sizeof(BLOCKS) / sizeof(BLOCKS[0]);

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static int sb_putn(strbuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = realloc(b->data, cap);
        if (!p) return -1;
        b->data = p;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int sb_puts(strbuf *b, const char *s) {
    return sb_putn(b, s, strlen(s));
}

static const char *find_scalar(const rp_values *v, const char *name, size_t namelen) {
    for (size_t i = 0; i < v->nscalars; i++) {
        if (strlen(v->scalars[i].name) == namelen && memcmp(v->scalars[i].name, name, namelen) == 0) {
            return v->scalars[i].value;
        }
    }
    return "";
}

static const rp_block *find_block(const rp_values *v, const char *name, size_t namelen) {
    for (size_t i = 0; i < v->nblocks; i++) {
        if (strlen(v->blocks[i].name) == namelen && memcmp(v->blocks[i].name, name, namelen) == 0) {
            return &v->blocks[i];
        }
    }
    return NULL;
}

/* 0 unknown, 1 scalar, 2 block. */
static int classify(const char *name, size_t namelen) {
    for (size_t i = 0; i < NSCALARS; i++) {
        if (strlen(SCALARS[i]) == namelen && memcmp(SCALARS[i], name, namelen) == 0) return 1;
    }
    for (size_t i = 0; i < NBLOCKS; i++) {
        if (strlen(BLOCKS[i]) == namelen && memcmp(BLOCKS[i], name, namelen) == 0) return 2;
    }
    return 0;
}

/*
 * A marker is "{{" + name (no braces, no whitespace) + spaces + "}}".
 * Mirrors MARKER = /\{\{([^{}\s]+)( *)\}\}/: a tab before }} matches nothing.
 */
static int parse_marker(const char *p, const char *end, size_t *namelen, size_t *spaces, size_t *mlen) {
    if (end - p < 5 || p[0] != '{' || p[1] != '{') return 0;
    const char *q = p + 2;
    while (q < end && *q != '{' && *q != '}' && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') q++;
    if (q == p + 2) return 0;
    *namelen = (size_t)(q - (p + 2));
    const char *r = q;
    while (r < end && *r == ' ') r++;
    *spaces = (size_t)(r - q);
    if (end - r < 2 || r[0] != '}' || r[1] != '}') return 0;
    *mlen = (size_t)(r + 2 - p);
    return 1;
}

/* Text outside a block marker may be whitespace or drawing characters only:
 * a letter or digit there means the block was written inline in a sentence. */
static int is_frame(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 0;
    }
    return 1;
}

static void fail(char *errmsg, size_t errcap, int *errline, int line, const char *fmt, const char *name,
                 size_t namelen) {
    char namebuf[64];
    size_t n = namelen < sizeof(namebuf) - 1 ? namelen : sizeof(namebuf) - 1;
    memcpy(namebuf, name, n);
    namebuf[n] = '\0';
    snprintf(errmsg, errcap, fmt, line, namebuf);
    *errline = line;
}

/* Pads a value to a fixed-width marker's width, or refuses one that fits not. */
static int fit(strbuf *b, const char *name, const char *value, size_t spaces, size_t width, int line, char *errmsg,
               size_t errcap, int *errline) {
    if (spaces == 0) return sb_puts(b, value);
    size_t vlen = strlen(value);
    if (vlen > width) {
        snprintf(errmsg, errcap, "line %d: value for %s is %lu characters, field is %lu", line, name,
                 (unsigned long)vlen, (unsigned long)width);
        *errline = line;
        return -1;
    }
    if (sb_puts(b, value) != 0) return -1;
    for (size_t i = vlen; i < width; i++) {
        if (sb_putn(b, " ", 1) != 0) return -1;
    }
    return 0;
}

static int render_scalar_line(strbuf *b, const char *text, const char *end, const rp_values *v, int line,
                              char *errmsg, size_t errcap, int *errline) {
    const char *p = text;
    while (p < end) {
        size_t namelen, spaces, mlen;
        if (parse_marker(p, end, &namelen, &spaces, &mlen)) {
            const char *name = p + 2;
            int kind = classify(name, namelen);
            if (kind == 0) {
                fail(errmsg, errcap, errline, line, "line %d: unknown placeholder {{%s}}", name, namelen);
                return -1;
            }
            if (kind == 2) {
                fail(errmsg, errcap, errline, line, "line %d: block placeholder {{%s}} must stand alone on its line",
                     name, namelen);
                return -1;
            }
            char namebuf[64];
            size_t n = namelen < sizeof(namebuf) - 1 ? namelen : sizeof(namebuf) - 1;
            memcpy(namebuf, name, n);
            namebuf[n] = '\0';
            if (fit(b, namebuf, find_scalar(v, name, namelen), spaces, mlen, line, errmsg, errcap, errline) != 0) {
                return -1;
            }
            p += mlen;
        } else {
            if (sb_putn(b, p, 1) != 0) return -1;
            p++;
        }
    }
    return 0;
}

static int render_block_line(strbuf *b, const char *text, size_t len, const rp_values *v, int line, char *errmsg,
                             size_t errcap, int *errline, const char *name, size_t namelen, size_t pos, size_t spaces,
                             size_t mlen) {
    size_t prelen = pos;
    size_t suflen = len - (pos + mlen);
    if (!is_frame(text, prelen) || !is_frame(text + pos + mlen, suflen)) {
        fail(errmsg, errcap, errline, line, "line %d: block placeholder {{%s}} must stand alone on its line", name,
             namelen);
        return -1;
    }
    const rp_block *blk = find_block(v, name, namelen);
    size_t nlines = blk ? blk->nlines : 0;
    char namebuf[64];
    size_t n = namelen < sizeof(namebuf) - 1 ? namelen : sizeof(namebuf) - 1;
    memcpy(namebuf, name, n);
    namebuf[n] = '\0';
    for (size_t i = 0; i < nlines; i++) {
        const char *value = blk->lines[i];
        if (i > 0 && sb_putn(b, "\n", 1) != 0) return -1;
        strbuf row = { NULL, 0, 0 };
        int rc = sb_putn(&row, text, prelen) != 0
                || fit(&row, namebuf, value, spaces, mlen, line, errmsg, errcap, errline) != 0
                || sb_putn(&row, text + pos + mlen, suflen) != 0;
        if (rc != 0) {
            free(row.data);
            return -1;
        }
        if (value[0] == '\0') {
            /* An empty value must not leave the frame's padding behind as
             * trailing whitespace: trim the whole framed line, which only
             * strips anything when the suffix is blank. */
            while (row.len > 0 && (row.data[row.len - 1] == ' ' || row.data[row.len - 1] == '\t')) row.len--;
            row.data[row.len] = '\0';
        }
        rc = sb_puts(b, row.data ? row.data : "");
        free(row.data);
        if (rc != 0) return -1;
    }
    return 0;
}

char *rp_render_template(const char *tpl, const rp_values *v, int *errline, char *errmsg, size_t errcap) {
    strbuf b = { NULL, 0, 0 };
    *errline = 0;
    if (errcap > 0) errmsg[0] = '\0';
    int line = 1;
    const char *p = tpl;
    int first = 1;
    for (;;) {
        const char *end = strchr(p, '\n');
        int last = end == NULL;
        const char *lend = last ? p + strlen(p) : end;
        if (!first && sb_putn(&b, "\n", 1) != 0) goto oom;
        first = 0;
        /* One scan per line: unknown names fail up front, and a block marker
         * anywhere but alone on its line fails, like the desktop renderer. */
        size_t count = 0;
        const char *bname = NULL;
        size_t bnamelen = 0, bspaces = 0, bmlen = 0, bpos = 0;
        int bad = 0;
        const char *q = p;
        while (q < lend) {
            size_t namelen, spaces, mlen;
            if (parse_marker(q, lend, &namelen, &spaces, &mlen)) {
                const char *name = q + 2;
                int kind = classify(name, namelen);
                if (kind == 0) {
                    fail(errmsg, errcap, errline, line, "line %d: unknown placeholder {{%s}}", name, namelen);
                    bad = 1;
                    break;
                }
                count++;
                if (kind == 2 && bname == NULL) {
                    bname = name;
                    bnamelen = namelen;
                    bspaces = spaces;
                    bmlen = mlen;
                    bpos = (size_t)(q - p);
                }
                q += mlen;
            } else {
                q++;
            }
        }
        if (bad) {
            free(b.data);
            return NULL;
        }
        int rc;
        if (bname != NULL) {
            if (count > 1) {
                fail(errmsg, errcap, errline, line, "line %d: block placeholder {{%s}} must stand alone on its line",
                     bname, bnamelen);
                free(b.data);
                return NULL;
            }
            rc = render_block_line(&b, p, (size_t)(lend - p), v, line, errmsg, errcap, errline, bname, bnamelen, bpos,
                                  bspaces, bmlen);
        } else {
            rc = render_scalar_line(&b, p, lend, v, line, errmsg, errcap, errline);
        }
        if (rc != 0) {
            free(b.data);
            return NULL;
        }
        if (last) break;
        p = end + 1;
        line++;
    }
    if (!b.data) {
        b.data = malloc(1);
        if (!b.data) goto oom;
        b.data[0] = '\0';
    }
    return b.data;
oom:
    snprintf(errmsg, errcap, "out of memory");
    *errline = line;
    free(b.data);
    return NULL;
}

void rp_normalise_line_endings(char *text) {
    char *w = text;
    for (char *r = text; *r; r++) {
        if (*r == '\r') {
            if (*(r + 1) == '\n') r++;
            *w++ = '\n';
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}
