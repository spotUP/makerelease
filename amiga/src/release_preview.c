#include "release_preview.h"

#include <stdio.h>

size_t rp_release_preview_view(const char *diz, const char *nfo, char *out, size_t cap) {
    int n = snprintf(out, cap, "---- file_id.diz ----\n%s\n\n---- release NFO ----\n%s", diz ? diz : "",
                     nfo ? nfo : "(this group has no NFO template)");
    return n < 0 ? 0 : (size_t)n;
}
