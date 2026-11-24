#include "credits.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

/* Trims [s, e) in place and returns the length; *start is the new start. */
static size_t trim_span(const char *s, const char *e, const char **start) {
    while (s < e && isspace((unsigned char)*s)) s++;
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *start = s;
    return (size_t)(e - s);
}

static int eq_nocase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int contains_nocase(const char *s, const char *needle) {
    size_t n = strlen(needle);
    for (; *s; s++) {
        size_t i = 0;
        while (i < n && s[i] && tolower((unsigned char)s[i]) == needle[i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

static void put_field(char *dst, size_t cap, const char *s, size_t len) {
    if (len > cap) len = cap;
    memcpy(dst, s, len);
    dst[len] = '\0';
}

size_t rp_parse_credits(const char *text, rp_credit *out, size_t max) {
    size_t n = 0;
    const char *p = text;
    while (*p && n < max) {
        const char *end = p, *colon = NULL, *bar = NULL, *role, *q;
        size_t rlen;
        while (*end && *end != ';' && *end != '\n' && *end != '\r') {
            if (*end == ':' && !colon) colon = end;
            if (*end == '|' && !bar) bar = end;
            end++;
        }
        if (bar) colon = bar; /* a credits table row "Role | Name": the role may hold a ':' */
        if (colon) {
            rlen = trim_span(p, colon, &role);
            q = colon + 1;
            while (rlen && q <= end && n < max) {
                const char *comma = q, *name;
                size_t nlen;
                while (comma < end && *comma != ',') comma++;
                nlen = trim_span(q, comma, &name);
                if (nlen) {
                    put_field(out[n].role, RP_CREDIT_ROLE_MAX, role, rlen);
                    put_field(out[n].name, RP_CREDIT_NAME_MAX, name, nlen);
                    n++;
                }
                q = comma + 1;
            }
        }
        p = *end ? end + 1 : end;
    }
    return n;
}

static int role_hit(const char *role, int kind) {
    static const char *const coder[] = {"cod", "program", "dev", NULL};
    static const char *const design[] = {"design", "graphic", "art", "ink", "logo", "ansi", "ascii", NULL};
    const char *const *t = kind == RP_ROLE_CODER ? coder : design;
    if (kind < 0) return 1;
    for (; *t; t++) if (contains_nocase(role, *t)) return 1;
    return 0;
}

/* kind < 0: every role. Dedupe only against credits that passed the same filter. */
static char *join_names(const rp_credit *credits, size_t n, int kind) {
    strbuf b = {0};
    size_t i, j;
    for (i = 0; i < n; i++) {
        int dup = 0;
        if (!credits[i].name[0] || !role_hit(credits[i].role, kind)) continue;
        for (j = 0; j < i && !dup; j++) dup = role_hit(credits[j].role, kind) && eq_nocase(credits[j].name, credits[i].name);
        if (dup) continue;
        if (b.len && sb_puts(&b, ", ")) goto oom;
        if (sb_puts(&b, credits[i].name)) goto oom;
    }
    if (sb_puts(&b, "")) goto oom;
    return b.data;
oom:
    free(b.data);
    return NULL;
}

char *rp_creators_from_credits(const rp_credit *credits, size_t n) { return join_names(credits, n, -1); }

char *rp_credit_names_for_role(const rp_credit *credits, size_t n, rp_role_kind kind) {
    return join_names(credits, n, (int)kind);
}

const char *rp_credits_error(const rp_credit *credits, size_t n, int needs_creator_line) {
    size_t i;
    if (!needs_creator_line) return NULL;
    for (i = 0; i < n; i++) if (credits[i].name[0]) return NULL;
    return "Add at least one credit - the release history creator line is built from them.";
}

/* True when skill [s, s+len) equals one of the comma-separated skills in role, ignoring case. */
static int has_skill(const char *role, const char *s, size_t len) {
    while (*role) {
        const char *comma = role, *k;
        size_t klen;
        while (*comma && *comma != ',') comma++;
        klen = trim_span(role, comma, &k);
        if (klen == len) {
            size_t i = 0;
            while (i < len && tolower((unsigned char)k[i]) == tolower((unsigned char)s[i])) i++;
            if (i == len) return 1;
        }
        role = *comma ? comma + 1 : comma;
    }
    return 0;
}

size_t rp_merge_member_skills(rp_member *members, size_t n) {
    size_t out = 0, i, j;
    for (i = 0; i < n; i++) {
        const char *nick, *p;
        size_t nlen = trim_span(members[i].nickname, members[i].nickname + strlen(members[i].nickname), &nick);
        size_t off = (size_t)(nick - members[i].nickname);
        rp_member src = members[i]; /* members[out] can be this very row, so read from a copy */
        rp_member *dst = NULL;
        if (!nlen) continue;
        for (j = 0; j < out; j++) {
            if (strlen(members[j].nickname) == nlen && memcmp(members[j].nickname, src.nickname + off, nlen) == 0) {
                dst = &members[j];
                break;
            }
        }
        if (!dst) {
            dst = &members[out++];
            memcpy(dst->nickname, src.nickname + off, nlen);
            dst->nickname[nlen] = '\0';
            dst->role[0] = '\0';
        }
        for (p = src.role; *p;) {
            const char *comma = p, *skill;
            size_t slen, cur = strlen(dst->role);
            while (*comma && *comma != ',') comma++;
            slen = trim_span(p, comma, &skill);
            if (slen && !has_skill(dst->role, skill, slen) && (cur ? cur + 2 + slen : slen) <= RP_ROLE_MAX) {
                if (cur) { dst->role[cur++] = ','; dst->role[cur++] = ' '; }
                memcpy(dst->role + cur, skill, slen);
                dst->role[cur + slen] = '\0';
            }
            p = *comma ? comma + 1 : comma;
        }
    }
    return out;
}
