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

/* The Code field as typed, desktop rules (renderer/main.ts: input upper-cases, maxLength = codeLength, submit takes
 * ^[A-Z0-9]{n}$): letters upper-cased, anything but A-Z and 0-9 dropped, cut to length characters. */
void rp_code_filter(const char *in, int length, char *out, size_t cap);

#define RP_CODE_NEAR_LEN 16
/* Free-code candidates near a taken code, nearest first, each as long as code (the desktop has no such list): a
 * trailing number steps up then down (FB006 -> FB007 FB005 FB008 ...), a code without one swaps its last character
 * for 2..9 (HYPOC -> HYPO2 HYPO3 ...). Writes at most max; returns the count. */
int rp_code_near(const char *code, char (*out)[RP_CODE_NEAR_LEN], int max);

/* Lower-cases ASCII letters into out (catalog file names are lower case). */
void rp_lower(const char *in, char *out, size_t cap);

/* Base name + ".LHA" (lha != 0) or ".ZIP". */
void rp_archive_name(const char *prefix, const char *code, int lha, char *out, size_t cap);

#endif
