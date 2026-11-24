#ifndef RP_ARCHIVE_PREVIEW_H
#define RP_ARCHIVE_PREVIEW_H

#include <stddef.h>

/* A selected folder that only wraps one more folder, and the folder to use instead. */
typedef struct {
    char *path;
    char *flatten_to;
} rp_nested;

typedef struct {
    char **entries; /* archive listing: REL/, REL/file_id.diz, REL/REL.NFO, then the files */
    size_t nentries;
    rp_nested *nested;
    size_t nnested;
} rp_preview;

/* Same listing and nested rule as the desktop previewArchive. Returns 0, or -1 with err set. */
int rp_preview_archive(const char *release, const char *const *paths, size_t npaths, rp_preview *out,
                       char *err, size_t errcap);
void rp_preview_free(rp_preview *p);

#endif
