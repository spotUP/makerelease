#include "aminet_short.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static int short_is(const char *desc, const char *want) {
    char out[64];
    rp_aminet_short_prefill(desc, out, sizeof out);
    return strcmp(out, want) == 0;
}

int main(void) {
    char small[8];
    CHECK(short_is("", "") && short_is("   ", ""));
    CHECK(short_is("  A short line  ", "A short line"));
    /* exactly 40 characters stays whole */
    CHECK(short_is("1234567890123456789012345678901234567890", "1234567890123456789012345678901234567890"));
    /* 41 characters with the space as the 41st: the 40 before it stay */
    CHECK(short_is("1234567890123456789012345678901234567890 tail", "1234567890123456789012345678901234567890"));
    /* longer: cut at the last word boundary inside 40 */
    CHECK(short_is("A tracker for chip music with a very long description", "A tracker for chip music with a very"));
    CHECK(short_is("Amiga music disk with songs by many artists", "Amiga music disk with songs by many"));
    /* one long word is cut hard */
    CHECK(short_is("12345678901234567890123456789012345678901234", "1234567890123456789012345678901234567890"));
    /* output buffer limit */
    rp_aminet_short_prefill("Hello brave world", small, sizeof small);
    CHECK(strcmp(small, "Hello b") == 0);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_aminet_short: ok\n");
    return 0;
}
