#ifndef RP_NAMING_H
#define RP_NAMING_H

#include <stddef.h>

/* The name every artefact shares: prefix + upper-case code. */
void rp_base_name(const char *prefix, const char *code, char *out, size_t cap);

/* Upper-cases ASCII letters into out (codes are [A-Z0-9]). */
void rp_upper(const char *in, char *out, size_t cap);

/* Code guessed from a release name, as app/src/shared/code-suggest.ts: initials of the words, then the trailing
 * number ("Fleisch Bitte AD 006", 5 -> "FB006"); initials are cut to fit, a short result is padded from the first word. */
void rp_suggest_code(const char *name, int length, char *out, size_t cap);

/* Lower-cases ASCII letters into out (catalog file names are lower case). */
void rp_lower(const char *in, char *out, size_t cap);

/* Base name + ".LHA" (lha != 0) or ".ZIP". */
void rp_archive_name(const char *prefix, const char *code, int lha, char *out, size_t cap);

#endif
