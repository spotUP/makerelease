#include "nfo_x.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"
#include "wrap.h"

#define FIELD_WIDTH 59
#define NOTES_WIDTH (FIELD_WIDTH - 1)
#define LIST_ART_INDENT 7
#define BOARD_NAME_COL 2
#define BOARD_SYSOP_COL 22
#define BOARD_HQ_END 36
#define BOARD_STATUS_END 56

static const char *const GUTTER_LEFT = "   | %% |";
static const char *const GUTTER_RIGHT = "| %% |";

static const char *const TEMPLATE =
#include "nfo_x_template.inc"
    ;

static const char *const BANNER =
    "    _ _ __ ___    ____  _ _ __ _________\n"
    "   | | Y  / _ \\../ __ \\| | Y  / __  | | |\n"
    "dNo| | | | | | || /_ | | | | | |_ | l | |dS!\n"
    "|  | | | | |`| || || | | | | | || |___| |  |\n"
    "|  | | l | |__/'| || | | | | | || |   | |  |\n"
    "|  | l___| |    | || | | l l | ll | | | |  |\n"
    "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |\n"
    "|     uP rOUGH /X iNNOVATiONS pRESENTS:    |\n"
    "|                                          |\n"
    "`------------------------------------------'";

const char *rp_x_banner(void) {
    return BANNER;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* ---- DIZ release line ---- */

static int is_frame_char(char c) {
    return !is_ws(c) && !isalnum((unsigned char)c);
}

static char *dup_n(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (p) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

char *rp_diz_apply_release_line(const char *banner, int row, const char *text) {
    size_t nlines = 1, i, ls, le, rlen, start, end, tail, width, tlen, left, pad;
    const char *p;
    int clamped = row;
    char *out;
    strbuf b = { NULL, 0, 0 };
    for (p = banner; *p; p++)
        if (*p == '\n') nlines++;
    if (clamped > (int)nlines - 1) clamped = (int)nlines - 1;
    if (clamped < 0) clamped = 0;
    ls = 0;
    for (i = 0; i < (size_t)clamped; i++) ls += strcspn(banner + ls, "\n") + 1;
    le = ls + strcspn(banner + ls, "\n");
    rlen = le - ls;
    while (rlen > 0 && (banner[ls + rlen - 1] == ' ' || banner[ls + rlen - 1] == '\t')) rlen--;
    for (start = 0; start < rlen && start < 3 && is_frame_char(banner[ls + start]); start++) {}
    for (tail = 0; tail < rlen && tail < 3 && is_frame_char(banner[ls + rlen - 1 - tail]); tail++) {}
    end = (rlen - tail > start) ? rlen - tail : rlen;
    width = end - start;
    /* before the row, untouched */
    if (sb_putn(&b, banner, ls) != 0) goto oom;
    /* head, interior (row text padded to end), centred text, tail */
    if (sb_putn(&b, banner + ls, start) != 0) goto oom;
    while (*text && is_ws(*text)) text++;
    tlen = strlen(text);
    while (tlen > 0 && is_ws(text[tlen - 1])) tlen--;
    if (tlen > width) tlen = width;
    pad = width - tlen;
    left = (pad + 1) / 2;
    for (i = 0; i < left; i++) sb_putn(&b, " ", 1);
    sb_putn(&b, text, tlen);
    for (i = 0; i < pad - left; i++) sb_putn(&b, " ", 1);
    if (sb_putn(&b, banner + ls + end, rlen - end) != 0) goto oom;
    if (sb_puts(&b, banner + le) != 0) goto oom;
    out = b.data ? b.data : dup_n("", 0);
    return out;
oom:
    free(b.data);
    return NULL;
}

void rp_x_release_name_in_banner(const char *banner, char *out, size_t cap) {
    const char *p = banner;
    int announced = 0;
    out[0] = '\0';
    /* announced = line after the first line holding "presents:" (any case), else all lines */
    for (;;) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p), k;
        for (k = 0; k + 9 <= len && !announced; k++) {
            static const char want[] = "presents:";
            size_t j = 0;
            while (j < 9 && tolower((unsigned char)p[k + j]) == want[j]) j++;
            announced = (j == 9);
        }
        if (announced || !e) break;
        p = e + 1;
    }
    if (announced) {
        const char *e = strchr(p, '\n');
        if (!e) return;
        p = e + 1;
    } else {
        p = banner;
    }
    for (;;) {
        const char *e = strchr(p, '\n');
        size_t a = 0, z = e ? (size_t)(e - p) : strlen(p), k;
        int dashes = 1;
        while (a < z && strchr("|`'\\/", p[a])) a++;
        while (z > a && strchr("|`'\\/", p[z - 1])) z--;
        while (a < z && is_ws(p[a])) a++;
        while (z > a && is_ws(p[z - 1])) z--;
        for (k = a; k < z; k++)
            if (!strchr("-_=. \t\n\r\f\v", p[k])) dashes = 0;
        if (z > a && !dashes) {
            size_t n = z - a;
            if (n > cap - 1) n = cap - 1;
            memcpy(out, p + a, n);
            out[n] = '\0';
            return;
        }
        if (!e) return;
        p = e + 1;
    }
}

char *rp_x_list_art(const char *banner) {
    char name[256];
    rp_x_release_name_in_banner(banner, name, sizeof(name));
    return rp_diz_apply_release_line(BANNER, RP_X_RELEASE_ROW, name);
}

/* The desktop's DIZ line source: "<name> at <place>" when Released at is filled; the diz is returned unchanged
 * (a copy) when either is empty, so the group's own line stays until both exist. */
char *rp_diz_with_released_at(const char *diz, const char *name, const char *place) {
    char line[256];
    while (is_ws(*name)) name++;
    while (is_ws(*place)) place++;
    if (*name == '\0' || *place == '\0') return dup_n(diz, strlen(diz));
    snprintf(line, sizeof line, "%s at %s", name, place);
    return rp_diz_apply_release_line(diz, RP_X_RELEASE_ROW, line);
}

/* A typed Diz line replaces the release line as it is (clipped to the slot by rp_diz_apply_release_line); blank falls back to Released at. */
char *rp_diz_with_line(const char *diz, const char *name, const char *place, const char *line) {
    char t[256];
    size_t n;
    while (is_ws(*line)) line++;
    n = strlen(line);
    while (n > 0 && is_ws(line[n - 1])) n--;
    if (n == 0) return rp_diz_with_released_at(diz, name, place);
    if (n >= sizeof t) n = sizeof t - 1;
    memcpy(t, line, n);
    t[n] = '\0';
    return rp_diz_apply_release_line(diz, RP_X_RELEASE_ROW, t);
}

/* ---- NFO ---- */

static int sb_nl(strbuf *b) {
    return b->len > 0 ? sb_putn(b, "\n", 1) : 0;
}

static int sb_pad(strbuf *b, size_t n) {
    while (n--)
        if (sb_putn(b, " ", 1) != 0) return -1;
    return 0;
}

/* One framed content line: text clipped/padded to the field. */
static int frame(strbuf *b, const char *text) {
    size_t n = strlen(text);
    if (n > FIELD_WIDTH) n = FIELD_WIDTH;
    return sb_nl(b) || sb_puts(b, GUTTER_LEFT) || sb_putn(b, text, n) || sb_pad(b, FIELD_WIDTH - n) ||
           sb_puts(b, GUTTER_RIGHT);
}

static int entry_rule(strbuf *b) {
    size_t i;
    if (sb_nl(b) || sb_puts(b, "- -| %% |")) return -1;
    for (i = 0; i < FIELD_WIDTH; i++)
        if (sb_putn(b, "-", 1) != 0) return -1;
    return sb_puts(b, "| %% |- -");
}

/* Release name line, "<<-- name -->>", left-heavy centred. */
static int name_line(strbuf *b, const char *name) {
    strbuf t = { NULL, 0, 0 };
    size_t n, pad, left;
    int rc;
    if (sb_puts(&t, "<<-- ") || sb_puts(&t, name) || sb_puts(&t, " -->>")) {
        free(t.data);
        return -1;
    }
    n = t.len > FIELD_WIDTH ? FIELD_WIDTH : t.len;
    pad = FIELD_WIDTH - n;
    left = (pad + 1) / 2;
    {
        strbuf f = { NULL, 0, 0 };
        rc = sb_pad(&f, left) || sb_putn(&f, t.data, n) || sb_pad(&f, pad - left) || frame(b, f.data ? f.data : "");
        free(f.data);
    }
    free(t.data);
    return rc;
}

static char *fit_field(const char *v, size_t width) {
    char *p = malloc(width + 1);
    size_t n = strlen(v);
    if (!p) return NULL;
    if (n > width) n = width;
    memcpy(p, v, n);
    memset(p + n, ' ', width - n);
    p[width] = '\0';
    return p;
}

static int notes_block(strbuf *b, const char *notes) {
    const char *p = notes;
    for (;;) {
        size_t len = strcspn(p, "\n"), n;
        const char *e = p[len] ? p + len : NULL;
        int rc = 0;
        /* \r\n and \r are line ends the desktop normalises to \n */
        n = len;
        while (n > 0 && is_ws(p[n - 1])) n--;
        if (n <= NOTES_WIDTH) {
            strbuf l = { NULL, 0, 0 };
            rc = sb_putn(&l, " ", 1) || sb_putn(&l, p, n) || frame(b, l.data);
            free(l.data);
        } else {
            size_t ind = 0, w = 0;
            strbuf cur = { NULL, 0, 0 };
            int have = 0;
            while (ind < n && is_ws(p[ind])) ind++;
            w = ind;
            while (!rc && w < n) {
                size_t ws, we;
                while (w < n && is_ws(p[w])) w++;
                ws = w;
                while (w < n && !is_ws(p[w])) w++;
                we = w;
                if (we == ws) break;
                if (!have) {
                    cur.len = 0;
                    rc = sb_putn(&cur, p, ind) || sb_putn(&cur, p + ws, we - ws);
                    have = 1;
                } else if (cur.len + 1 + (we - ws) > NOTES_WIDTH) {
                    strbuf l = { NULL, 0, 0 };
                    rc = sb_putn(&l, " ", 1) || sb_putn(&l, cur.data, cur.len) || frame(b, l.data);
                    free(l.data);
                    cur.len = 0;
                    rc = rc || sb_putn(&cur, p, ind) || sb_putn(&cur, p + ws, we - ws);
                } else {
                    rc = sb_putn(&cur, " ", 1) || sb_putn(&cur, p + ws, we - ws);
                }
            }
            if (!rc && have) {
                strbuf l = { NULL, 0, 0 };
                rc = sb_putn(&l, " ", 1) || sb_putn(&l, cur.data, cur.len) || frame(b, l.data);
                free(l.data);
            }
            free(cur.data);
        }
        if (rc) return -1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

static int news_block(strbuf *b, const char *news) {
    rp_lines raw = { NULL, 0 };
    size_t first = 0, last, i;
    int rc = 0;
    /* split on LF; each line trimmed (CR is whitespace) */
    {
        const char *p = news;
        for (;;) {
            size_t len = strcspn(p, "\n"), a = 0, z = len;
            char **nl;
            char *c;
            while (a < z && is_ws(p[a])) a++;
            while (z > a && is_ws(p[z - 1])) z--;
            c = dup_n(p + a, z - a);
            nl = realloc(raw.lines, (raw.n + 1) * sizeof *nl);
            if (!c || !nl) {
                free(c);
                if (nl) raw.lines = nl;
                rp_free_lines(&raw);
                return -1;
            }
            raw.lines = nl;
            raw.lines[raw.n++] = c;
            if (!p[len]) break;
            p += len + 1;
        }
    }
    last = raw.n;
    while (first < last && raw.lines[first][0] == '\0') first++;
    while (last > first && raw.lines[last - 1][0] == '\0') last--;
    if (first == last) {
        rp_free_lines(&raw);
        return 0;
    }
    rc = frame(b, "") || frame(b, "");
    for (i = first; i < last && !rc; i++) {
        strbuf f = { NULL, 0, 0 };
        size_t n = strlen(raw.lines[i]), pad, left;
        if (n > FIELD_WIDTH) n = FIELD_WIDTH;
        pad = FIELD_WIDTH - n;
        left = (pad + 1) / 2;
        rc = sb_pad(&f, left) || sb_putn(&f, raw.lines[i], n) || sb_pad(&f, pad - left) || frame(b, f.data ? f.data : "");
        free(f.data);
    }
    rc = rc || frame(b, "") || frame(b, "");
    rp_free_lines(&raw);
    return rc;
}

/* "sysops" -> "s Y S O P S" */
static int spaced_caps(strbuf *o, const char *role) {
    size_t a = 0, z = strlen(role), i;
    while (a < z && is_ws(role[a])) a++;
    while (z > a && is_ws(role[z - 1])) z--;
    for (i = a; i < z; i++) {
        char c = role[i];
        char ch = (c == 'i' || c == 'I' || i == a) ? (char)tolower((unsigned char)c) : (char)toupper((unsigned char)c);
        if (i > a && sb_putn(o, " ", 1) != 0) return -1;
        if (sb_putn(o, &ch, 1) != 0) return -1;
    }
    return 0;
}

/* Next comma-separated skill of role (trimmed, non-empty). Returns 0 at the end. */
static int next_skill(const char **cur, char *out, size_t cap) {
    const char *p = *cur;
    while (*p) {
        size_t len = strcspn(p, ","), a = 0, z = len;
        const char *next = p[len] ? p + len + 1 : p + len;
        while (a < z && is_ws(p[a])) a++;
        while (z > a && is_ws(p[z - 1])) z--;
        if (z > a) {
            size_t n = z - a < cap - 1 ? z - a : cap - 1;
            memcpy(out, p + a, n);
            out[n] = '\0';
            *cur = next;
            return 1;
        }
        p = next;
    }
    *cur = p;
    return 0;
}

static int roster_block(strbuf *b, const rp_member *m, size_t nm) {
    char **keys = NULL;
    size_t nkeys = 0, k, i, labelw = 0;
    int rc = 0;
    char skill[RP_ROLE_MAX + 1];
    if (nm == 0) return 0;
    keys = calloc(nm * (RP_ROLE_MAX + 1), sizeof *keys);
    if (!keys) return -1;
    for (i = 0; i < nm && !rc; i++) {
        const char *cur = m[i].role;
        while (next_skill(&cur, skill, sizeof(skill))) {
            for (k = 0; k < nkeys; k++)
                if (strcmp(keys[k], skill) == 0) break;
            if (k == nkeys) {
                keys[nkeys] = dup_n(skill, strlen(skill));
                if (!keys[nkeys]) {
                    rc = -1;
                    break;
                }
                nkeys++;
            }
        }
    }
    for (k = 0; k < nkeys && !rc; k++) {
        strbuf s = { NULL, 0, 0 };
        rc = spaced_caps(&s, keys[k]);
        if (!rc && s.len > labelw) labelw = s.len;
        free(s.data);
    }
    for (k = 0; k < nkeys && !rc; k++) {
        strbuf caps = { NULL, 0, 0 }, line = { NULL, 0, 0 };
        size_t contw, nth = 0;
        if (k > 0) rc = frame(b, "");
        rc = rc || spaced_caps(&caps, keys[k]) || sb_puts(&line, "  ") || sb_pad(&line, labelw - caps.len) ||
             sb_putn(&line, caps.data ? caps.data : "", caps.len) || sb_puts(&line, " >> ");
        contw = line.len;
        for (i = 0; i < nm && !rc; i++) {
            const char *cur = m[i].role;
            while (!rc && next_skill(&cur, skill, sizeof(skill))) {
                size_t nl;
                if (strcmp(skill, keys[k]) != 0) continue;
                nl = strlen(m[i].nickname);
                if (nth > 0) nl += 3;
                if (line.len + nl > FIELD_WIDTH - 1) {
                    rc = frame(b, line.data);
                    line.len = 0;
                    rc = rc || sb_pad(&line, contw) || sb_puts(&line, m[i].nickname);
                } else {
                    rc = (nth > 0 ? sb_puts(&line, " . ") : 0) || sb_puts(&line, m[i].nickname);
                }
                nth++;
            }
        }
        rc = rc || frame(b, line.data);
        free(caps.data);
        free(line.data);
    }
    for (k = 0; k < nkeys; k++) free(keys[k]);
    free(keys);
    return rc;
}

static int dots(strbuf *o, size_t from, size_t to) {
    while (to > from++)
        if (sb_putn(o, ".", 1) != 0) return -1;
    return 0;
}

static int boards_block(strbuf *b, const rp_x_board *bd, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        strbuf l = { NULL, 0, 0 };
        size_t hq = strlen(bd[i].hq), st = strlen(bd[i].status);
        int rc = sb_pad(&l, BOARD_NAME_COL) || sb_puts(&l, bd[i].name) || sb_puts(&l, " ") ||
                 dots(&l, l.len, BOARD_SYSOP_COL - 1) || sb_puts(&l, " ") || sb_puts(&l, bd[i].sysop) ||
                 sb_puts(&l, " ") || dots(&l, l.len, BOARD_HQ_END > hq ? BOARD_HQ_END - hq - 1 : 0) ||
                 sb_puts(&l, " ") || sb_puts(&l, bd[i].hq) || sb_puts(&l, " ") ||
                 dots(&l, l.len, BOARD_STATUS_END > st ? BOARD_STATUS_END - st - 1 : 0) || sb_puts(&l, " ") ||
                 sb_puts(&l, bd[i].status) || frame(b, l.data);
        free(l.data);
        if (rc) return -1;
    }
    return 0;
}

static int list_block(strbuf *b, const rp_x_entry *e, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        const char *p = e[i].art;
        strbuf l = { NULL, 0, 0 };
        int rc = entry_rule(b) || frame(b, "") || sb_puts(&l, "  ") || sb_puts(&l, e[i].number) || sb_puts(&l, ":") ||
                 frame(b, l.data);
        free(l.data);
        for (;;) {
            size_t len = strcspn(p, "\n");
            strbuf a = { NULL, 0, 0 };
            if (rc) return -1;
            rc = sb_pad(&a, LIST_ART_INDENT) || sb_putn(&a, p, len) || frame(b, a.data);
            free(a.data);
            if (!p[len]) break;
            p += len + 1;
        }
        if (rc || frame(b, "") || frame(b, "")) return -1;
    }
    return entry_rule(b) || frame(b, "") || frame(b, "");
}

/* Replace the first occurrence of marker in *s with repl, or - when drop is
 * set - remove the marker's own line (and its LF). */
static int replace_first(char **s, const char *marker, const char *repl, int drop) {
    size_t ml = strlen(marker), rl = repl ? strlen(repl) : 0;
    char *at, *out;
    if (drop) {
        for (at = *s; (at = strstr(at, marker)) != NULL; at++)
            if (at == *s || at[-1] == '\n') break;
        if (!at) return 0;
        rl = 0;
        if (at[ml] == '\n') ml++;
    } else if (!(at = strstr(*s, marker))) {
        return 0;
    }
    out = malloc(strlen(*s) - ml + rl + 1);
    if (!out) return -1;
    memcpy(out, *s, (size_t)(at - *s));
    if (!drop) memcpy(out + (at - *s), repl, rl);
    strcpy(out + (at - *s) + rl, at + ml);
    free(*s);
    *s = out;
    return 0;
}

char *rp_render_nfo_x(const rp_nfo_x_form *f) {
    char *s = dup_n(TEMPLATE, strlen(TEMPLATE)), *fld;
    strbuf nm = { NULL, 0, 0 };
    strbuf blk[5];
    static const char *const markers[5] = { "{{NOTES}}", "{{NEWS}}", "{{ROSTER}}", "{{BOARDS}}", "{{RELEASE_LIST}}" };
    static const char *const names[4] = { "{{DATE}}", "{{CODER}}", "{{SYSTEM}}", "{{DESIGN}}" };
    const char *vals[4];
    int i, rc = 0;
    memset(blk, 0, sizeof blk);
    if (!s) return NULL;
    vals[0] = f->release_date;
    vals[1] = f->coder;
    vals[2] = f->system;
    vals[3] = f->design;
    rc = name_line(&nm, f->release_name) || replace_first(&s, "{{RELEASE_NAME_LINE}}", nm.data, 0);
    for (i = 0; i < 4 && !rc; i++) {
        fld = fit_field(vals[i], 10);
        rc = !fld || replace_first(&s, names[i], fld, 0);
        free(fld);
    }
    rc = rc || notes_block(&blk[0], f->notes) || news_block(&blk[1], f->news) ||
         roster_block(&blk[2], f->members, f->nmembers) || boards_block(&blk[3], f->boards, f->nboards) ||
         list_block(&blk[4], f->entries, f->nentries);
    for (i = 0; i < 5 && !rc; i++) rc = replace_first(&s, markers[i], blk[i].data, blk[i].len == 0);
    for (i = 0; i < 5; i++) free(blk[i].data);
    free(nm.data);
    if (rc) {
        free(s);
        return NULL;
    }
    return s;
}
