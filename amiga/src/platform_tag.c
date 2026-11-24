#include "platform_tag.h"

#include <ctype.h>
#include <string.h>

const char *const rp_platform_labels[RP_PLATFORM_COUNT + 1] = { "None", "A500", "A1200", "Console", "Other", NULL };

int rp_platform_wants_text(int index) { return index == 3 || index == 4; }

void rp_platform_tag(int index, const char *text, char *out, size_t cap) {
    size_t n;
    if (cap == 0) return;
    out[0] = '\0';
    if (index < 1 || index >= RP_PLATFORM_COUNT) return;
    if (rp_platform_wants_text(index)) {
        while (isspace((unsigned char)*text)) text++;
        n = strlen(text);
        while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
        if (n == 0 && index == 3) text = "Console", n = 7;
    } else {
        text = rp_platform_labels[index];
        n = strlen(text);
    }
    if (n >= cap) n = cap - 1;
    memcpy(out, text, n);
    out[n] = '\0';
}
