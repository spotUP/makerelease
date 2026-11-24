#include "text_rows.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "credits.h"
#include "strbuf.h"

#define ROW_FIELDS_MAX 4

typedef struct {
    const char *s[ROW_FIELDS_MAX];
    size_t len[ROW_FIELDS_MAX];
    int n;
} row_fields;

static int fail(rp_row_err *err, size_t line, const char *msg) {
    if (err) {
        err->line = line;
        snprintf(err->msg, sizeof err->msg, "%s", msg);
    }
    return -1;
}

/* Next line of *p (advances it); 0 at the end of the text. */
static int next_line(const char **p, const char **ls, const char **le) {
    const char *q = *p;
    if (!*q) return 0;
    *ls = q;
    while (*q && *q != '\n' && *q != '\r') q++;
    *le = q;
    if (*q == '\r' && q[1] == '\n') q++;
    if (*q) q++;
    *p = q;
    return 1;
}

/* Splits [s, e) at '|', trims each field. -1 when there are more than max. */
static int split_fields(const char *s, const char *e, row_fields *f, int max) {
    f->n = 0;
    for (;;) {
        const char *c = s, *a, *b;
        while (c < e && *c != '|') c++;
        if (f->n == max) return -1;
        a = s;
        b = c;
        while (a < b && isspace((unsigned char)*a)) a++;
        while (b > a && isspace((unsigned char)b[-1])) b--;
        f->s[f->n] = a;
        f->len[f->n] = (size_t)(b - a);
        f->n++;
        if (c >= e) return f->n;
        s = c + 1;
    }
}

static int put_field(char *dst, size_t cap, const row_fields *f, int i) {
    size_t len = i < f->n ? f->len[i] : 0;
    if (len > cap) return -1;
    if (len) memcpy(dst, f->s[i], len);
    dst[len] = '\0';
    return 0;
}

static int nonblank(const row_fields *f) {
    int i;
    for (i = 0; i < f->n; i++)
        if (f->len[i]) return 1;
    return 0;
}

/* Shared row loop: skips blank lines, hands each split row to fill(). */
typedef int (*fill_fn)(void *out, size_t idx, const row_fields *f, const char **msg);

static int parse_rows(const char *text, void *out, size_t elem, size_t max, int maxf, fill_fn fill,
                      rp_row_err *err, const char *fields_msg) {
    const char *p = text ? text : "", *ls, *le;
    size_t line = 0, n = 0;
    row_fields f;
    while (next_line(&p, &ls, &le)) {
        const char *msg = NULL;
        int r;
        line++;
        if (split_fields(ls, le, &f, maxf) < 0) return fail(err, line, fields_msg);
        if (!nonblank(&f)) continue;
        if (n >= max) return fail(err, line, "Too many rows.");
        r = fill((char *)out + n * elem, n, &f, &msg);
        if (r < 0) return fail(err, line, msg);
        if (r > 0) n++;
    }
    return (int)n;
}

/* ---- format helpers ---- */

static int put_clean(strbuf *b, const char *s) {
    for (; *s; s++) {
        char c = *s == '|' ? '/' : (*s == '\n' || *s == '\r') ? ' ' : *s;
        if (sb_putn(b, &c, 1)) return -1;
    }
    return 0;
}

/* Joins fields with " | ", dropping trailing empty ones; one row per line. */
static int put_row(strbuf *b, const char *const *v, int n) {
    int i;
    while (n > 1 && !*v[n - 1]) n--;
    for (i = 0; i < n; i++) {
        if (i && sb_puts(b, " | ")) return -1;
        if (put_clean(b, v[i])) return -1;
    }
    return sb_puts(b, "\n");
}

static char *finish(strbuf *b, int bad) {
    if (bad) { free(b->data); return NULL; }
    if (!b->data) return strdup("");
    return b->data;
}

/* ---- members ---- */

static int fill_member(void *o, size_t idx, const row_fields *f, const char **msg) {
    rp_member *m = o;
    (void)idx;
    if (!f->len[0]) { *msg = "Nickname is missing."; return -1; }
    if (put_field(m->nickname, RP_NICK_MAX, f, 0)) { *msg = "Nickname is too long."; return -1; }
    if (put_field(m->role, RP_ROLE_MAX, f, 1)) { *msg = "Role is too long."; return -1; }
    return 1;
}

static int cmp_nick(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d) return d;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int rp_parse_member_rows(const char *text, rp_member *out, size_t max, rp_row_err *err) {
    int n = parse_rows(text, out, sizeof *out, max, 2, fill_member, err, "Use: Nickname | role");
    size_t i, k;
    if (n < 0) return n;
    n = (int)rp_merge_member_skills(out, (size_t)n);
    for (i = 1; i < (size_t)n; i++) { /* stable insertion sort */
        rp_member t = out[i];
        for (k = i; k > 0 && cmp_nick(out[k - 1].nickname, t.nickname) > 0; k--) out[k] = out[k - 1];
        out[k] = t;
    }
    return n;
}

char *rp_format_member_rows(const rp_member *members, size_t n) {
    strbuf b = {0};
    int bad = 0;
    size_t i;
    for (i = 0; i < n && !bad; i++) {
        const char *v[2];
        v[0] = members[i].nickname;
        v[1] = members[i].role;
        bad = put_row(&b, v, 2);
    }
    return finish(&b, bad);
}

/* ---- boards ---- */

static int fill_board(void *o, size_t idx, const row_fields *f, const char **msg) {
    rp_board_row *r = o;
    (void)idx;
    if (!f->len[0]) { *msg = "Board name is missing."; return -1; }
    if (put_field(r->name, RP_BOARD_NAME_MAX, f, 0)) { *msg = "Board name is too long."; return -1; }
    if (put_field(r->sysop, RP_BOARD_SYSOP_MAX, f, 1)) { *msg = "Sysop is too long."; return -1; }
    if (put_field(r->hq, RP_BOARD_HQ_MAX, f, 2)) { *msg = "HQ is too long."; return -1; }
    if (put_field(r->status, RP_BOARD_STATUS_MAX, f, 3)) { *msg = "Status is too long."; return -1; }
    return 1;
}

int rp_parse_board_rows(const char *text, rp_board_row *out, size_t max, rp_row_err *err) {
    return parse_rows(text, out, sizeof *out, max, 4, fill_board, err, "Use: Name | Sysop | HQ | Status");
}

void rp_board_rows_view(const rp_board_row *rows, size_t n, rp_rel_board *out) {
    size_t i;
    for (i = 0; i < n; i++) {
        out[i].name = rows[i].name;
        out[i].sysop = rows[i].sysop;
        out[i].hq = rows[i].hq;
        out[i].status = rows[i].status;
    }
}

char *rp_format_board_rows(const rp_rel_board *boards, size_t n) {
    strbuf b = {0};
    int bad = 0;
    size_t i;
    for (i = 0; i < n && !bad; i++) {
        const char *v[4];
        v[0] = boards[i].name ? boards[i].name : "";
        v[1] = boards[i].sysop ? boards[i].sysop : "";
        v[2] = boards[i].hq ? boards[i].hq : "";
        v[3] = boards[i].status ? boards[i].status : "";
        bad = put_row(&b, v, 4);
    }
    return finish(&b, bad);
}

/* ---- media embeds ---- */

static int eq_nocase(const char *s, size_t len, const char *word) {
    size_t i;
    if (strlen(word) != len) return 0;
    for (i = 0; i < len; i++)
        if (tolower((unsigned char)s[i]) != word[i]) return 0;
    return 1;
}

static int fill_embed(void *o, size_t idx, const row_fields *f, const char **msg) {
    rp_embed_row *r = o;
    int image;
    (void)idx;
    if (eq_nocase(f->s[0], f->len[0], "youtube")) image = 0;
    else if (eq_nocase(f->s[0], f->len[0], "image")) image = 1;
    else { *msg = "Type must be youtube or image."; return -1; }
    r->is_image = image;
    r->url[0] = r->path[0] = '\0';
    if (put_field(r->caption, RP_EMBED_CAPTION_MAX, f, 1)) { *msg = "Caption is too long."; return -1; }
    if (image) {
        if (!(f->n > 2 && f->len[2])) {
            *msg = "Every image media embed needs a picked image file - add its path or remove the row.";
            return -1;
        }
        if (put_field(r->path, RP_EMBED_PATH_MAX, f, 2)) { *msg = "Image path is too long."; return -1; }
        return 1;
    }
    if (!(f->n > 2 && f->len[2])) return 0; /* empty youtube row is skipped, as on the desktop */
    if (put_field(r->url, RP_EMBED_URL_MAX, f, 2)) { *msg = "YouTube URL is too long."; return -1; }
    return 1;
}

int rp_parse_embed_rows(const char *text, rp_embed_row *out, size_t max, rp_row_err *err) {
    return parse_rows(text, out, sizeof *out, max, 3, fill_embed, err, "Use: youtube | caption | url  or  image | caption | path");
}

void rp_embed_rows_view(const rp_embed_row *rows, size_t n, rp_rel_embed *out) {
    size_t i;
    for (i = 0; i < n; i++) {
        out[i].type = rows[i].is_image ? "image" : "youtube";
        out[i].caption = rows[i].caption;
        out[i].youtube_url = rows[i].is_image ? NULL : rows[i].url;
    }
}

char *rp_format_embed_rows(const rp_embed_row *rows, size_t n) {
    strbuf b = {0};
    int bad = 0;
    size_t i;
    for (i = 0; i < n && !bad; i++) {
        const char *v[3];
        v[0] = rows[i].is_image ? "image" : "youtube";
        v[1] = rows[i].caption;
        v[2] = rows[i].is_image ? rows[i].path : rows[i].url;
        bad = put_row(&b, v, 3);
    }
    return finish(&b, bad);
}

char *rp_embed_append_image(const char *text, const char *path) {
    const char *leaf, *q;
    size_t tl, pl, ll, cl;
    char *out, *w;
    if (!text) text = "";
    if (!path || !*path) return NULL;
    pl = strlen(path);
    if (pl > RP_EMBED_PATH_MAX || strpbrk(path, "|\r\n")) return NULL;
    leaf = path;
    for (q = path; *q; q++)
        if (*q == ':' || *q == '/') leaf = q + 1;
    ll = strlen(leaf);
    cl = ll > RP_EMBED_CAPTION_MAX ? RP_EMBED_CAPTION_MAX : ll;
    tl = strlen(text);
    out = malloc(tl + 1 + 8 + cl + 3 + pl + 2);
    if (!out) return NULL;
    w = out;
    memcpy(w, text, tl);
    w += tl;
    if (tl && text[tl - 1] != '\n' && text[tl - 1] != '\r') *w++ = '\n';
    memcpy(w, "image | ", 8);
    w += 8;
    memcpy(w, leaf, cl);
    w += cl;
    memcpy(w, " | ", 3);
    w += 3;
    memcpy(w, path, pl);
    w += pl;
    *w++ = '\n';
    *w = '\0';
    return out;
}

/* ---- row table ---- */

void rp_rtab_free(rp_rtab *t) {
    size_t i;
    for (i = 0; i < t->n; i++) free(t->row[i]);
    free(t->row);
    t->row = NULL;
    t->n = t->cap = 0;
}

static int rtab_push(rp_rtab *t, char *s) {
    if (!s) return -1;
    if (t->n == t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 16;
        char **r = realloc(t->row, cap * sizeof *r);
        if (!r) { free(s); return -1; }
        t->row = r;
        t->cap = cap;
    }
    t->row[t->n++] = s;
    return (int)t->n - 1;
}

static char *dup_range(const char *s, const char *e) {
    char *o = malloc((size_t)(e - s) + 1);
    if (o) { memcpy(o, s, (size_t)(e - s)); o[e - s] = '\0'; }
    return o;
}

int rp_rtab_set_text(rp_rtab *t, const char *text) {
    const char *p = text ? text : "", *ls, *le;
    rp_rtab_free(t);
    while (next_line(&p, &ls, &le))
        if (rtab_push(t, dup_range(ls, le)) < 0) { rp_rtab_free(t); return -1; }
    return 0;
}

char *rp_rtab_text(const rp_rtab *t) {
    strbuf b = {0};
    int bad = 0;
    size_t i;
    for (i = 0; i < t->n && !bad; i++) bad = sb_puts(&b, t->row[i]) || sb_puts(&b, "\n");
    return finish(&b, bad);
}

void rp_rtab_cell(const rp_rtab *t, size_t row, int col, char *out, size_t cap) {
    const char *s, *e;
    int c;
    if (!cap) return;
    out[0] = '\0';
    if (row >= t->n || col < 0 || col >= t->ncols) return;
    s = t->row[row];
    for (c = 0; c < col; c++) {
        s = strchr(s, '|');
        if (!s) return;
        s++;
    }
    e = col == t->ncols - 1 ? s + strlen(s) : s + strcspn(s, "|");
    while (s < e && isspace((unsigned char)*s)) s++;
    while (e > s && isspace((unsigned char)e[-1])) e--;
    if ((size_t)(e - s) >= cap) e = s + cap - 1;
    memcpy(out, s, (size_t)(e - s));
    out[e - s] = '\0';
}

int rp_rtab_set_row(rp_rtab *t, size_t row, const char *const *v) {
    strbuf b = {0};
    char *s;
    if (row >= t->n || put_row(&b, v, t->ncols)) { free(b.data); return -1; }
    s = finish(&b, 0);
    if (!s) return -1;
    s[strcspn(s, "\n")] = '\0';
    free(t->row[row]);
    t->row[row] = s;
    return 0;
}

int rp_rtab_add(rp_rtab *t) { return rtab_push(t, strdup("")); }

void rp_rtab_remove(rp_rtab *t, size_t row) {
    if (row >= t->n) return;
    free(t->row[row]);
    memmove(t->row + row, t->row + row + 1, (t->n - row - 1) * sizeof *t->row);
    t->n--;
}
