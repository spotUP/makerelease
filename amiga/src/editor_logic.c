#include "editor_logic.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text_rows.h"

int rp_editor_set_field(char *dst, size_t cap, const char *src, const char *name, char *err, size_t errcap) {
    size_t len;
    if (src == NULL) src = "";
    while (*src != '\0' && isspace((unsigned char)*src)) src++;
    len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) len--;
    if (len >= cap) {
        snprintf(err, errcap, "%s is too long (at most %lu characters).", name, (unsigned long)(cap - 1));
        return -1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return 0;
}

int rp_editor_code_length(const char *s, int *out, char *err, size_t errcap) {
    char *end;
    long v;
    if (s == NULL) s = "";
    while (isspace((unsigned char)*s)) s++;
    v = strtol(s, &end, 10);
    while (isspace((unsigned char)*end)) end++;
    if (end == s || *end != '\0' || v < 1 || v > 32) {
        snprintf(err, errcap, "Code length must be a whole number from 1 to 32.");
        return -1;
    }
    *out = (int)v;
    return 0;
}

int rp_editor_check_id(const char *s, char *err, size_t errcap) {
    if (s != NULL && rp_is_group_id(s)) return 0;
    snprintf(err, errcap, "Id must be 1 to %d characters: a-z, 0-9 and \"-\".", RP_ID_MAX);
    return -1;
}

int rp_editor_members_parse(const char *text, rp_member *out, size_t max, size_t *n, char *err, size_t errcap) {
    rp_row_err re;
    int r = rp_parse_member_rows(text, out, max, &re);
    if (r < 0) {
        snprintf(err, errcap, "Members: Line %lu: %s", (unsigned long)re.line, re.msg);
        return -1;
    }
    *n = (size_t)r;
    return 0;
}

int rp_editor_members_format(const rp_member *m, size_t n, char *out, size_t cap) {
    char *t = rp_format_member_rows(m, n);
    size_t len;
    if (t == NULL) return -1;
    len = strlen(t);
    if (len >= cap) { free(t); return -1; }
    memcpy(out, t, len + 1);
    free(t);
    return 0;
}
