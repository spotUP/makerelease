#ifndef RP_PLATFORM_TAG_H
#define RP_PLATFORM_TAG_H

#include <stddef.h>

/* The Platform dropdown of the desktop app (index.html platform-tag): None, A500, A1200, Console, Other. Console and Other ask for a
 * free text (which console, e.g. "Mega Drive"; the tag for Other, e.g. "CD32"). The tag feeds the NFO/diz system line, the online
 * entry and the Aminet type guess. */
#define RP_PLATFORM_COUNT 5
extern const char *const rp_platform_labels[RP_PLATFORM_COUNT + 1]; /* NULL terminated, for the cycle gadget */

/* 1 when the entry takes a free text (Console, Other). */
int rp_platform_wants_text(int index);
/* main.ts platformTagText: the tag for a dropdown index and the free text. Other gives the trimmed text (empty = no tag), Console the
 * trimmed text or "Console", the rest their label ("" for None). An index out of range gives "". */
void rp_platform_tag(int index, const char *text, char *out, size_t cap);

#endif
