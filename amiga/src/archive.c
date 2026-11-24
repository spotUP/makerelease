#include "archive.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *rp_archive_cmd(const char *bin, const char *archive, const char *base) {
    int n;
    char *cmd;
    if (strcmp(bin, "zip") == 0) {
        n = snprintf(NULL, 0, "\"%s\" -r \"%s\" \"%s\"", bin, archive, base);
    } else {
        n = snprintf(NULL, 0, "\"%s\" -r a \"%s\" \"%s\"", bin, archive, base);
    }
    if (n < 0) return NULL;
    cmd = malloc((size_t)n + 1);
    if (!cmd) return NULL;
    if (strcmp(bin, "zip") == 0) {
        snprintf(cmd, (size_t)n + 1, "\"%s\" -r \"%s\" \"%s\"", bin, archive, base);
    } else {
        snprintf(cmd, (size_t)n + 1, "\"%s\" -r a \"%s\" \"%s\"", bin, archive, base);
    }
    return cmd;
}
