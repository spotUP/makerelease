/* AmiExpress door repository client; mirrors app/src/main/door-repo-client.ts
 * submitDoor (multipart POST of one archive, no key, server wording passed
 * through). Pure logic over rp_http_fn. */
#ifndef RP_DOOR_REPO_CLIENT_H
#define RP_DOOR_REPO_CLIENT_H

#include "http.h"

#define RP_DOOR_REPO_SUBMISSIONS_URL "https://doors.uprough.net/api/door-repo/submissions"

typedef struct {
    int ok;
    char message[256];  /* what the door server said, in its own words */
    long id;            /* -1 when the server sent none */
} rp_door_result;

/* url NULL = RP_DOOR_REPO_SUBMISSIONS_URL. filename: base name only. boundary_seed
 * feeds the multipart boundary. Returns 0 when ok, -1 otherwise (res filled either way). */
int rp_door_submit(rp_http_fn http, void *ctx, const char *url, const char *filename,
                   const char *data, size_t len, unsigned long boundary_seed, rp_door_result *res);

#endif
