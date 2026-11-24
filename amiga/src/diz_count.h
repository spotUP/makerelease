#ifndef RP_DIZ_COUNT_H
#define RP_DIZ_COUNT_H

#include <stddef.h>

/* The per-row character counter under the Diz line (desktop renderDizPreview; label is the division's row label): the text that goes on the row is the typed line
 * when there is one, else "<name> at <place>" when both are set, else the name. Writes
 *   "LABEL: N/WIDTH characters."                                              (fits)
 *   "LABEL: N characters, the diz fits WIDTH; the rest is cut off."           (too long)
 * into out (cap bytes, NUL-terminated). Surrounding blanks are not counted. */
void rp_diz_line_counter(const char *label, size_t width, const char *line, const char *name, const char *place, char *out, size_t cap);

#endif
