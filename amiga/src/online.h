/* Online actions behind the UI buttons (publish, revoke, door repository),
 * mirroring app/src/main/backend-doorserver.ts. Pure logic over rp_http_fn plus
 * stdio reads, so host tests drive it with a fake transport. All text the UI
 * hands in is Latin-1; it is converted to UTF-8 here before the form is built. */
#ifndef RP_ONLINE_H
#define RP_ONLINE_H

#include <stddef.h>

#include "credits.h"
#include "groups.h"
#include "http.h"
#include "json.h"
#include "release_seed.h"

/* Latin-1 -> UTF-8. Returns 0, or -1 when cap is too small (out is then cut). */
int rp_latin1_to_utf8(const char *in, char *out, size_t cap);

typedef struct {
    const rp_group *group;
    const char *code, *release_name, *platform_tag, *description, *release_notes;
    const char *news;          /* multi-line news text, LF separated; NULL = "" */
    const char *creators;      /* release history creator line; NULL = "" */
    const char *category;      /* history entry category (rp_release_categories); required */
    const rp_credit *credits;  /* role/name rows from the Authors field; n_credits may be 0 */
    size_t n_credits;
    const char *format;        /* "LHA" or "ZIP" */
    const char *diz_path;      /* the built file_id.diz */
    const char *archive_path;  /* the built archive; its base name is the upload name */
    const char *image_path;    /* the cover image, required like on the desktop */
    unsigned long seed;        /* multipart boundary seed */
    rp_http_fn http;
    void *http_ctx;
} rp_online_publish_args;

/* Each returns 0 when the server accepted, -1 otherwise; msg is the one status
 * line to show (desktop wording where the desktop has it). */
int rp_online_publish(const rp_online_publish_args *a, char *msg, size_t cap);
/* The server seed for a group: next release number, merged roster, warnings
 * (rp_seed_parse). Returns 0 with *tree set (the caller json_frees it once done
 * with info->current_diz / boards), or -1 with msg when the group has no release
 * server or the server could not be asked. now_seconds is Unix seconds. */
/* True when the group has a release server this build can reach (a door server profile with a known division). */
#define RP_ONLINE_HAS_SERVER(g) ((g)->backend_doorserver && ((g)->renderer == R_UPROUGH || (g)->renderer == R_XINNOVATIONS))

int rp_online_seed(const rp_group *g, rp_http_fn http, void *http_ctx, long now_seconds, rp_seed_info *info,
                   jnode **tree, char *msg, size_t cap);
/* The "code already taken" check (rp_code_check) for a group. 1 taken, 0 free or
 * the group has no server (msg ""), -1 server could not be asked (msg = error). */
int rp_online_code_check(const rp_group *g, const char *code, rp_http_fn http, void *http_ctx, char *msg, size_t cap);
int rp_online_revoke(const rp_group *g, const char *code, rp_http_fn http, void *http_ctx, char *msg, size_t cap);
/* Only /X Innovations submits to the door repository (submitsToDoorRepo). */
int rp_online_door(const rp_group *g, const char *archive_path, unsigned long seed, rp_http_fn http,
                   void *http_ctx, char *msg, size_t cap);

#endif
