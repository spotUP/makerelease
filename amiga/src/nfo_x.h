#ifndef RP_NFO_X_H
#define RP_NFO_X_H

#include <stddef.h>

#include "groups.h"

/* The /X Innovations NFO and its DIZ release line, byte for byte like
 * app/src/main/nfo-x-innovations.ts and app/src/shared/diz-release-line.ts. */

typedef struct {
    const char *name, *sysop, *hq, *status;
} rp_x_board;

typedef struct {
    const char *number; /* display number, e.g. "oo7" */
    const char *art;    /* banner rows joined by LF */
} rp_x_entry;

typedef struct {
    const char *release_name, *release_date, *coder, *system, *design;
    const char *notes, *news;
    const rp_member *members;
    size_t nmembers;
    const rp_x_board *boards;
    size_t nboards;
    const rp_x_entry *entries; /* newest first */
    size_t nentries;
} rp_nfo_x_form;

/* The division's default ten-row DIZ banner. */
const char *rp_x_banner(void);
/* editableDizRows[0].row of the division. */
#define RP_X_RELEASE_ROW 8

/* Returns malloc'd text, or NULL on out of memory. */
char *rp_render_nfo_x(const rp_nfo_x_form *form);

/* applyReleaseLineText: centre text in the slot between the row's frame
 * characters. Returns malloc'd banner. */
char *rp_diz_apply_release_line(const char *banner, int row, const char *text);

/* releaseNameInBanner: copies the name into out (cap bytes, NUL-terminated). */
void rp_x_release_name_in_banner(const char *banner, char *out, size_t cap);

/* listArtFromBanner: the division art with only the release name filled in. */
char *rp_x_list_art(const char *banner);

#endif
