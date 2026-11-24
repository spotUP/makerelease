#include "aminet_short.h"

#include "aminet.h"

#include <ctype.h>
#include <string.h>

void rp_aminet_short_prefill(const char *description, char *out, size_t cap) {
    size_t n, limit = RP_AMINET_SHORT_MAX;
    if (cap == 0) return;
    out[0] = '\0';
    while (isspace((unsigned char)*description)) description++;
    n = strlen(description);
    if (n > limit) {
        /* cut before the last space inside the limit; the character just past it may itself be the space */
        size_t i = limit;
        while (i > 0 && !isspace((unsigned char)description[i])) i--;
        n = i > 0 ? i : limit;
    }
    while (n > 0 && isspace((unsigned char)description[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    memcpy(out, description, n);
    out[n] = '\0';
}
