/* Port of app/src/main/stale-drafts.ts: the warning for builds that were
 * never published and never revoked (they keep their release number). */
#ifndef RP_STALE_DRAFTS_H
#define RP_STALE_DRAFTS_H

#include <stddef.h>

#include "json.h"

#define RP_STALE_DRAFT_AGE_SECONDS 86400L

/* drafts: the seed's "drafts" array (entries code, releaseNumber, createdAt in
 * Unix seconds), or NULL. Drafts younger than a day are ignored. Returns 1
 * with the warning in out, or 0 with out "" (also when nothing is stale).
 * Text is cut at cap-1. */
int rp_describe_stale_drafts(const jnode *drafts, long now_seconds, char *out, size_t cap);

#endif
