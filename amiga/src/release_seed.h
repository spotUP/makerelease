/* Logic around the release server's seed: the next release number, the
 * merged member roster, the boards, the history categories, and the warnings
 * the desktop shows when it opens a group (app/src/main/backend-doorserver.ts
 * seed(), renderer/main.ts init(), checkCode()). Pure over parsed JSON; HTTP
 * stays in releases_client. */
#ifndef RP_RELEASE_SEED_H
#define RP_RELEASE_SEED_H

#include <stddef.h>

#include "groups.h"
#include "json.h"
#include "releases_client.h"

/* The history entry category choices (index.html #entry-category), NULL ended. */
#define RP_RELEASE_CATEGORY_COUNT 11
extern const char *const rp_release_categories[RP_RELEASE_CATEGORY_COUNT + 1];
/* Index of an exact category name, or -1 (also for "" = nothing chosen). */
int rp_release_category_index(const char *name);

#define RP_SEED_BOARDS_MAX 16

typedef struct {
    long next_number;                 /* -1 when the seed has none */
    char number_text[16];             /* "217", "" when none */
    const char *current_diz;          /* into the seed tree; "" when null/absent */
    rp_member members[RP_MEMBERS_MAX];/* one per nickname (rp_merge_member_skills) */
    size_t nmembers;
    rp_rel_board boards[RP_SEED_BOARDS_MAX]; /* strings point into the seed tree */
    size_t nboards;
    char numbering_warning[400];      /* "" when none */
    char stale_warning[1200];
    char config_warning[240];
    char warnings[1900];              /* the three joined by a space, as the desktop status line */
} rp_seed_info;                       /* about 7 KB: keep it static or on the heap */

/* seed: the object from rp_rel_seed. numbering: the object from
 * rp_rel_numbering_check, or NULL when that call failed (it is a warning, not
 * a precondition). has_write_key: the profile has a server key. The seed tree
 * must outlive out (current_diz, boards). Returns 0, or -1 when seed is no
 * object. */
int rp_seed_parse(const jnode *seed, const jnode *numbering, long now_seconds,
                  const char *profile_id, int has_write_key, rp_seed_info *out);

/* The "code already taken" check behind the Release tab. base_name is the
 * release's file base name (rp_base_name). Returns 1 taken (msg = the
 * desktop warning), 0 free (msg ""), -1 when the server could not be asked
 * (msg = the error). The caller only asks once the code has full length. */
int rp_code_check(const rp_rel_cfg *c, const char *code, const char *base_name, char *msg, size_t cap);

/* The Release tab's number preview (desktop updateReleaseNumberPreview): "release 217 up-hypoc.lha". number_text "" prints
 * "?"; an empty code prints code_length X so the name shows its shape. Returns 0, or -1 when cap is too small. */
int rp_release_number_preview(const char *number_text, const char *file_prefix, const char *code, int code_length, int lha,
                              char *out, size_t cap);

/* Whether a group change may overwrite a prefilled field: 1 when the field is empty or still holds what the last prefill put
 * there (last_prefill "" when none), 0 when the user edited it. */
int rp_prefill_replaceable(const char *current, const char *last_prefill);

#endif
