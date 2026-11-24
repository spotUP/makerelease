#include "creators.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t rp_creators_token(const char *text, char *tok, size_t cap) {
    size_t start = 0;
    size_t i, len;
    const char *p;
    for (i = 0; text[i]; i++) {
        if (text[i] == ',' || text[i] == ';' || text[i] == ':' || text[i] == '\n') start = i + 1;
    }
    p = text + start;
    while (*p && isspace((unsigned char)*p)) p++;
    len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) len--;
    if (len >= cap) len = cap - 1;
    memcpy(tok, p, len);
    tok[len] = '\0';
    return start;
}

static int starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++, prefix++;
    }
    return 1;
}

size_t rp_creators_matches(const char *token, const rp_member *members, size_t n, const char **out, size_t max) {
    size_t i, k = 0;
    if (token[0] == '\0') return 0;
    for (i = 0; i < n && k < max; i++) {
        if (members[i].nickname[0] && starts_with_ci(members[i].nickname, token)) out[k++] = members[i].nickname;
    }
    return k;
}

int rp_creators_complete(const char *text, const rp_member *members, size_t n, char *out, size_t cap) {
    char tok[RP_NICK_MAX + 1];
    const char *hit[RP_CREATORS_SUGGEST_MAX];
    size_t start = rp_creators_token(text, tok, sizeof tok), k, i;
    const char *p = text + start;
    if (tok[0] == '\0') return 0;
    /* already a member (any case): the hint list is enough, nothing to expand */
    for (i = 0; i < n; i++) if (strlen(members[i].nickname) == strlen(tok) && starts_with_ci(members[i].nickname, tok)) return 0;
    k = rp_creators_matches(tok, members, n, hit, RP_CREATORS_SUGGEST_MAX);
    if (k != 1) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if ((size_t)(p - text) + strlen(hit[0]) + 1 > cap) return 0;
    memcpy(out, text, (size_t)(p - text));
    strcpy(out + (p - text), hit[0]);
    return 1;
}

void rp_creators_preview(const rp_credit *credits, size_t n, char *out, size_t cap) {
    char *all = rp_creators_from_credits(credits, n);
    snprintf(out, cap, "%s%s", all && all[0] ? "Release history creator line: " : "Release history creator line: add a credit with both a role and a name.", all ? all : "");
    free(all);
}

void rp_x_credits_preview(const rp_credit *credits, size_t n, char *out, size_t cap) {
    char *coder = rp_credit_names_for_role(credits, n, RP_ROLE_CODER), *design = rp_credit_names_for_role(credits, n, RP_ROLE_DESIGN);
    int some = (coder && coder[0]) || (design && design[0]);
    snprintf(out, cap, "NFO header: cODER [%s]  dESIGN [%s]%s", coder && coder[0] ? coder : "-", design && design[0] ? design : "-",
             some ? "" : " - add credits with roles like Code or Graphics");
    free(coder);
    free(design);
}
