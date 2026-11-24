#ifndef RP_AMINET_SHORT_H
#define RP_AMINET_SHORT_H

#include <stddef.h>

/* The Aminet Short field's prefill (desktop main.ts prefillAminetShort): the description when it fits RP_AMINET_SHORT_MAX characters,
 * else its first line cut at a word boundary (a single word longer than that is cut hard). Trimmed; empty for a blank description. */
void rp_aminet_short_prefill(const char *description, char *out, size_t cap);

#endif
