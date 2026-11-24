/* The Preview tab text: the generated file_id.diz and release NFO as one block for the read-only text box. */
#ifndef RP_RELEASE_PREVIEW_H
#define RP_RELEASE_PREVIEW_H

#include <stddef.h>

/* Writes "file_id.diz", the diz, "release NFO", the nfo (nfo NULL: a note that the group has none), cut at
 * cap-1 bytes plus NUL. Returns the full untruncated length, so a result >= cap means the text was cut. */
size_t rp_release_preview_view(const char *diz, const char *nfo, char *out, size_t cap);

#endif
