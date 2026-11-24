#include "last_choice.h"

#include <stdio.h>
#include <string.h>

int rp_last_format_get(const rp_last_choice *c, const char *id, int *zip) {
    int i;
    for (i = 0; i < c->nfmt; i++) {
        if (strcmp(c->fmt_id[i], id) == 0) { *zip = c->fmt_zip[i]; return 1; }
    }
    return 0;
}

void rp_last_format_set(rp_last_choice *c, const char *id, int zip) {
    int i;
    for (i = 0; i < c->nfmt && strcmp(c->fmt_id[i], id) != 0; i++) {}
    if (i == c->nfmt) {
        if (i >= RP_LAST_FMT_MAX) return;
        snprintf(c->fmt_id[i], sizeof c->fmt_id[i], "%s", id);
        c->nfmt++;
    }
    c->fmt_zip[i] = zip;
}

int rp_last_choice_load(const char *path, rp_last_choice *c) {
    FILE *f;
    char line[RP_ID_MAX + 40];
    memset(c, 0, sizeof *c);
    f = fopen(path, "rb");
    if (f == NULL) return -1;
    if (fgets(line, sizeof line, f) != NULL) {
        size_t n = strcspn(line, "\r\n");
        if (n > RP_ID_MAX) n = RP_ID_MAX;
        memcpy(c->group, line, n);
        c->group[n] = '\0';
        if (fgets(line, sizeof line, f) != NULL) c->zip = strncmp(line, "ZIP", 3) == 0;
        while (fgets(line, sizeof line, f) != NULL) {
            char *sp = strchr(line, ' ');
            if (sp == NULL || sp == line || sp - line > RP_ID_MAX) continue;
            *sp = '\0';
            rp_last_format_set(c, line, strncmp(sp + 1, "ZIP", 3) == 0);
        }
    }
    fclose(f);
    return c->group[0] != '\0' ? 0 : -1;
}

int rp_last_choice_save(const char *path, const rp_last_choice *c) {
    FILE *f = fopen(path, "wb");
    int i;
    if (f == NULL) return -1;
    fprintf(f, "%s\n%s\n", c->group, c->zip ? "ZIP" : "LHA");
    for (i = 0; i < c->nfmt; i++) fprintf(f, "%s %s\n", c->fmt_id[i], c->fmt_zip[i] ? "ZIP" : "LHA");
    return fclose(f) == 0 ? 0 : -1;
}
