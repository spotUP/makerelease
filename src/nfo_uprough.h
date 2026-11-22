#ifndef RP_NFO_UPROUGH_H
#define RP_NFO_UPROUGH_H

#include <stddef.h>

#include "groups.h"

typedef struct {
    const char *number;
    const char *filenames;
    const char *title;
    const char *category;
    const char *creators;
} rp_history;

typedef struct {
    const char *release_name;
    const char *platform; /* "" when absent */
    const char *code;
    int format_lha;
    const char *release_date; /* "" when absent */
    const char *notes;
    const char *news;
    const rp_member *members;
    size_t nmembers;
} rp_nfo_form;

/* Up Rough's plain 80-column layout, byte for byte like the desktop
 * renderer. history[0] is the release itself. Returns malloc'd text. */
char *rp_render_nfo_uprough(const char *file_prefix, const rp_history *hist, size_t nhist, const rp_nfo_form *form,
                            char *err, size_t errcap);

/* The DIZ grid: trailing padding trimmed, ASCII-safe, one trailing newline. */
char *rp_render_diz(const char *banner, char *err, size_t errcap);

#endif
