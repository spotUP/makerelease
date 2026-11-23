#ifndef RP_CREDITS_H
#define RP_CREDITS_H

#include <stddef.h>

#include "groups.h"

/* Port of the desktop credit logic (app/src/renderer/credits-creators.ts,
 * credits-table.ts, app/src/shared/members.ts). The credit rows are the one
 * place a release's people are entered; the NFO history creator line and the
 * /X coder and design fields are derived from them. */

#define RP_CREDIT_ROLE_MAX 63
#define RP_CREDIT_NAME_MAX 63
#define RP_CREDITS_MAX 32

typedef struct {
    char role[RP_CREDIT_ROLE_MAX + 1];
    char name[RP_CREDIT_NAME_MAX + 1];
} rp_credit;

typedef enum { RP_ROLE_CODER, RP_ROLE_DESIGN } rp_role_kind;

/* Single-line entry format (the desktop only has a table of role/name rows,
 * so this is defined here): "Role: Name, Name; Role: Name". ';' or a newline
 * ends a group, ':' splits role from names, ',' separates names sharing the
 * role. Like the desktop table, a row needs both role and name, so empty
 * pieces and groups without ':' are dropped. Order is kept; fields longer
 * than the buffers are cut. Returns the number of credits written (<= max). */
size_t rp_parse_credits(const char *text, rp_credit *out, size_t max);

/* "Name, Name" in first-credit order, a name credited twice (case-insensitive)
 * appears once. malloc'd, "" when none; NULL on out of memory. */
char *rp_creators_from_credits(const rp_credit *credits, size_t n);

/* Same, restricted to roles matching kind (coder: cod|program|dev, design:
 * design|graphic|art|ink|logo|ansi|ascii, case-insensitive substring). */
char *rp_credit_names_for_role(const rp_credit *credits, size_t n, rp_role_kind kind);

/* Message blocking a build for missing credits, or NULL. */
const char *rp_credits_error(const rp_credit *credits, size_t n, int needs_creator_line);

/* One entry per nickname (trimmed, empty ones dropped), skills joined with
 * ", " in first-seen order without case-insensitive repeats; first-row order
 * kept. Works in place and returns the new count. A skill that no longer fits
 * rp_member.role (RP_ROLE_MAX) is dropped whole, never cut mid-word. */
size_t rp_merge_member_skills(rp_member *members, size_t n);

#endif
