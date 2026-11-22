#include "nfo_uprough.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "naming.h"
#include "wrap.h"

#define HIST_LABEL_W 12
#define HIST_INDENT 8
#define HIST_CONT_INDENT 20
#define HIST_WRAP_W 68
#define MEMBER_MIN_W 20

static const char *const REL_NOTES = "        [ r e l e a s e   n o t e s ]";
static const char *const NEWS_H = "        [ n e w s ]";
static const char *const MEMBERS_H = "        a c t i v e   u p   r o u g h   h e r o e s";
static const char *const HISTORY_H = "        r e l e a s e d   s o   f a r";
static const char *const ONLINE = "        online         www.uprough.net";
static const char *const FOOTER = "        c a t c h `e m   a l l   a t   www.uprough.net";

static const char *const BANNER[] = {
    "",
    "                                 .___            ._______",
    "                 ._______      __|_ /____      __|_     /     .____.",
    "              ___|_     /_____/_ _/     /_    /   /    /_.    |    |_____",
    "             _)   /    //  _   / \\       /.__/_  /    / _|____|_   |    (_",
    "             \\   /    //   \\  /___\\_____/_|_   \\______\\ \\_     /   _     /",
    "         - --/________\\     _/-diP-----/   /    /--_/    /    /____\\_____\\-->>>",
    "                     /______\\      _ _/        /   \\__________\\",
    "                                 \\ \\)\\_________\\ ",
    "                  ",
};
static const size_t NBANNER = sizeof(BANNER) / sizeof(BANNER[0]);

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static int sb_putn(strbuf *b, const char *s, size_t n) {
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

static int sb_puts(strbuf *b, const char *s) {
    return sb_putn(b, s, strlen(s));
}

/* Latin-1 transliterations for the BBS-safe DIZ, same table as desktop. */
static const char *ascii_for(unsigned char c) {
    switch (c) {
    case 0xE4: return "ae";
    case 0xF6: return "oe";
    case 0xE5: return "aa";
    case 0xFC: return "ue";
    case 0xE9:
    case 0xE8:
    case 0xEA: return "e";
    case 0xC4: return "Ae";
    case 0xD6: return "Oe";
    case 0xC5: return "Aa";
    case 0xDC: return "Ue";
    case 0xC9: return "E";
    case 0xF1: return "n";
    case 0xE7: return "c";
    case 0xF8: return "oe";
    case 0xD8: return "Oe";
    case 0xE6: return "ae";
    case 0xC6: return "Ae";
    default: return "?";
    }
}

static int sb_ascii(strbuf *b, const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c <= 0x7F) {
            if (sb_putn(b, s, 1) != 0) return -1;
        } else if (sb_puts(b, ascii_for(c)) != 0) {
            return -1;
        }
    }
    return 0;
}

static void emit_line(strbuf *b, int *first, const char *s) {
    if (!*first) sb_putn(b, "\n", 1);
    *first = 0;
    sb_puts(b, s);
}

/* indentBlock: every non-empty line gets 8 spaces. */
static int emit_indented(strbuf *b, int *first, const char *text) {
    const char *p = text;
    for (;;) {
        const char *end = strchr(p, '\n');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        char line[2048];
        if (n >= sizeof(line)) n = sizeof(line) - 1;
        if (n > 0) {
            memcpy(line, "        ", 8);
            memcpy(line + 8, p, n);
            line[8 + n] = '\0';
            emit_line(b, first, line);
        } else {
            emit_line(b, first, "");
        }
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static int emit_members(strbuf *b, int *first, const rp_member *members, size_t n) {
    size_t i, width = MEMBER_MIN_W;
    for (i = 0; i < n; i++) {
        size_t nl = strlen(members[i].nickname) + 2;
        if (nl > width) width = nl;
    }
    for (i = 0; i < n; i++) {
        char line[256];
        snprintf(line, sizeof(line), "        %-*s%s", (int)width, members[i].nickname, members[i].role);
        emit_line(b, first, line);
    }
    return 0;
}

static int emit_history(strbuf *b, int *first, const rp_history *h, size_t nh) {
    size_t i, k;
    const char *const labels[3] = { "title", "category", "creator(s)" };
    for (i = 0; i < nh; i++) {
        const char *const vals[3] = { h[i].title, h[i].category, h[i].creators };
        char head[256];
        snprintf(head, sizeof(head), "        release %s %s", h[i].number, h[i].filenames);
        emit_line(b, first, head);
        for (k = 0; k < 3; k++) {
            rp_lines w = rp_wrap_words(vals[k], HIST_WRAP_W - HIST_CONT_INDENT, HIST_WRAP_W - HIST_CONT_INDENT);
            size_t j;
            char line[512];
            snprintf(line, sizeof(line), "        %-*s%s", HIST_LABEL_W, labels[k], w.n > 0 ? w.lines[0] : "");
            emit_line(b, first, line);
            for (j = 1; j < w.n; j++) {
                snprintf(line, sizeof(line), "%*s%s", HIST_CONT_INDENT, "", w.lines[j]);
                emit_line(b, first, line);
            }
            rp_free_lines(&w);
        }
        if (i + 1 < nh) emit_line(b, first, "");
    }
    return 0;
}

static int all_space(const char *s) {
    for (; *s; s++) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
    }
    return 1;
}

char *rp_render_nfo_uprough(const char *file_prefix, const rp_history *hist, size_t nhist, const rp_nfo_form *form,
                            char *err, size_t errcap) {
    strbuf b = { NULL, 0, 0 };
    char archive[96], notes_line[512];
    size_t i;
    int first = 1;
    (void)err;
    (void)errcap;
    rp_archive_name(file_prefix, form->code, form->format_lha, archive, sizeof(archive));
    if (form->platform[0] != '\0') {
        snprintf(notes_line, sizeof(notes_line), "        Release named %s (%s) (%s)", form->release_name,
                 form->platform, archive);
    } else {
        snprintf(notes_line, sizeof(notes_line), "        Release named %s (%s)", form->release_name, archive);
    }
    for (i = 0; i < NBANNER; i++) emit_line(&b, &first, BANNER[i]);
    emit_line(&b, &first, "");
    emit_line(&b, &first, REL_NOTES);
    emit_line(&b, &first, "");
    emit_line(&b, &first, notes_line);
    if (form->release_date[0] != '\0' && !all_space(form->release_date)) {
        char dl[64];
        snprintf(dl, sizeof(dl), "        Released %s", form->release_date);
        emit_line(&b, &first, dl);
    }
    emit_line(&b, &first, "");
    emit_indented(&b, &first, form->notes);
    emit_line(&b, &first, "");
    emit_line(&b, &first, NEWS_H);
    emit_line(&b, &first, "");
    emit_indented(&b, &first, form->news);
    emit_line(&b, &first, "");
    emit_line(&b, &first, MEMBERS_H);
    emit_line(&b, &first, "");
    if (form->nmembers > 0) emit_members(&b, &first, form->members, form->nmembers);
    emit_line(&b, &first, "");
    emit_line(&b, &first, "");
    emit_line(&b, &first, ONLINE);
    emit_line(&b, &first, "");
    emit_line(&b, &first, "");
    emit_line(&b, &first, HISTORY_H);
    emit_line(&b, &first, "");
    if (nhist > 0) emit_history(&b, &first, hist, nhist);
    emit_line(&b, &first, "");
    emit_line(&b, &first, "");
    emit_line(&b, &first, FOOTER);
    emit_line(&b, &first, "");
    return b.data;
}

char *rp_render_diz(const char *banner, char *err, size_t errcap) {
    /* Split, rstrip each row, drop leading/trailing empty rows. */
    const char *p = banner;
    const char **rows = NULL;
    size_t n = 0, cap = 0, i, start = 0, end = 0;
    strbuf b = { NULL, 0, 0 };
    (void)err;
    (void)errcap;
    for (;;) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *row;
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
        row = malloc(len + 1);
        if (!row) goto oom;
        memcpy(row, p, len);
        row[len] = '\0';
        if (n == cap) {
            const char **nr;
            cap = cap ? cap * 2 : 16;
            nr = realloc(rows, cap * sizeof *nr);
            if (!nr) {
                free(row);
                goto oom;
            }
            rows = nr;
        }
        rows[n++] = row;
        if (!e) break;
        p = e + 1;
    }
    while (start < n && rows[start][0] == '\0') start++;
    end = n;
    while (end > start && rows[end - 1][0] == '\0') end--;
    for (i = start; i < end; i++) {
        if (i > start) sb_putn(&b, "\n", 1);
        sb_ascii(&b, rows[i]);
    }
    sb_putn(&b, "\n", 1);
    for (i = 0; i < n; i++) free((void *)rows[i]);
    free(rows);
    return b.data;
oom:
    for (i = 0; i < n; i++) free((void *)rows[i]);
    free(rows);
    free(b.data);
    return NULL;
}
