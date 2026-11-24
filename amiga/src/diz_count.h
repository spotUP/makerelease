#ifndef RP_DIZ_COUNT_H
#define RP_DIZ_COUNT_H

#include <stddef.h>

/* The per-row character counter under the Diz line (desktop renderDizPreview): the text that goes on the row is the typed line
 * when there is one, else "<name> at <place>" when both are set, else the name. Writes
 *   "Release line: N/WIDTH characters."                                              (fits)
 *   "Release line: N characters, the diz fits WIDTH; the rest is cut off."           (too long)
 * into out (cap bytes, NUL-terminated). Surrounding blanks are not counted. */
void rp_diz_line_counter(size_t width, const char *line, const char *name, const char *place, char *out, size_t cap);

#endif
