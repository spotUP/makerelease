#ifndef RP_CREATORS_H
#define RP_CREATORS_H

#include <stddef.h>

#include "credits.h"
#include "groups.h"

/* Port of the pure part of the desktop creators autocomplete (app/src/renderer/creators-autocomplete.ts, the single-name
 * mode a credit row's Name uses) and of the creators preview lines (main.ts updateCreatorsPreview). The Amiga credits
 * live in one line "Role: Name, Name; Role: Name", so the name being typed is the text after the last ',' ';' ':' or
 * newline. The member nicknames are the candidates. */

#define RP_CREATORS_SUGGEST_MAX 8 /* the desktop list shows 8 */

/* The name being typed (trimmed, copied to tok) and the offset in text where its untrimmed span starts. */
size_t rp_creators_token(const char *text, char *tok, size_t cap);

/* Nicknames whose start matches token (case-insensitive, empty token matches nothing), in member order, at most max.
 * Returns how many were written to out. */
size_t rp_creators_matches(const char *token, const rp_member *members, size_t n, const char **out, size_t max);

/* Desktop Enter/Tab pick, without a popup: when the typed name is not already a member and exactly one nickname starts with
 * it, writes text with that name replaced by the nickname to out and returns 1; otherwise returns 0 and out is untouched. */
int rp_creators_complete(const char *text, const rp_member *members, size_t n, char *out, size_t cap);

/* The preview under the credits: "Release history creator line: a, b" (or the add-a-credit hint), and for x_group the
 * NFO header fields too (coder / design roles), like the desktop's two preview lines. */
void rp_creators_preview(const rp_credit *credits, size_t n, int x_group, char *out, size_t cap);

#endif
