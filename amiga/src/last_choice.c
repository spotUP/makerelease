#include "last_choice.h"

#include <stdio.h>
#include <string.h>

int rp_last_choice_load(const char *path, rp_last_choice *c) {
    FILE *f;
    char line[RP_ID_MAX + 8];
    memset(c, 0, sizeof *c);
    f = fopen(path, "rb");
    if (f == NULL) return -1;
    if (fgets(line, sizeof line, f) != NULL) {
        size_t n = strcspn(line, "\r\n");
        if (n > RP_ID_MAX) n = RP_ID_MAX;
        memcpy(c->group, line, n);
        c->group[n] = '\0';
        if (fgets(line, sizeof line, f) != NULL) c->zip = strncmp(line, "ZIP", 3) == 0;
    }
    fclose(f);
    return c->group[0] != '\0' ? 0 : -1;
}

int rp_last_choice_save(const char *path, const rp_last_choice *c) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) return -1;
    fprintf(f, "%s\n%s\n", c->group, c->zip ? "ZIP" : "LHA");
    return fclose(f) == 0 ? 0 : -1;
}
