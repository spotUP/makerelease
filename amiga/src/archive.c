#include "archive.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *rp_archive_cmd(const char *bin, const char *archive, const char *base, int with_nfo) {
    const char *flags = strcmp(bin, "zip") == 0 ? "-r" : "-r a";
    const char *fmt = with_nfo ? "\"%s\" %s \"%s\" \"file_id.diz\" \"%s.NFO\" \"%s\""
                               : "\"%s\" %s \"%s\" \"file_id.diz\" \"%s\"";
    int n = with_nfo ? snprintf(NULL, 0, fmt, bin, flags, archive, base, base)
                     : snprintf(NULL, 0, fmt, bin, flags, archive, base);
    char *cmd;
    if (n < 0) return NULL;
    cmd = malloc((size_t)n + 1);
    if (!cmd) return NULL;
    if (with_nfo) snprintf(cmd, (size_t)n + 1, fmt, bin, flags, archive, base, base);
    else snprintf(cmd, (size_t)n + 1, fmt, bin, flags, archive, base);
    return cmd;
}
