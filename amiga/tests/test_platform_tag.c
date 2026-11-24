#include "platform_tag.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static int tag_is(int index, const char *text, const char *want) {
    char out[32];
    rp_platform_tag(index, text, out, sizeof out);
    return strcmp(out, want) == 0;
}

int main(void) {
    char small[4];
    CHECK(tag_is(0, "ignored", ""));
    CHECK(tag_is(1, "ignored", "A500"));
    CHECK(tag_is(2, "", "A1200"));
    /* Console keeps its detail, or falls back to the label */
    CHECK(tag_is(3, "  Mega Drive ", "Mega Drive"));
    CHECK(tag_is(3, "   ", "Console"));
    /* Other is exactly the text, empty = no tag */
    CHECK(tag_is(4, " CD32 ", "CD32"));
    CHECK(tag_is(4, "", ""));
    CHECK(tag_is(5, "CD32", "") && tag_is(-1, "CD32", ""));
    CHECK(!rp_platform_wants_text(0) && !rp_platform_wants_text(2) && rp_platform_wants_text(3) && rp_platform_wants_text(4));
    CHECK(rp_platform_labels[RP_PLATFORM_COUNT] == NULL && strcmp(rp_platform_labels[4], "Other") == 0);
    /* output is cut to the buffer */
    rp_platform_tag(4, "CD32-Amiga", small, sizeof small);
    CHECK(strcmp(small, "CD3") == 0);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_platform_tag: ok\n");
    return 0;
}
