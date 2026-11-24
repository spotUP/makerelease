#include "text_norm.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT(in, want) do { \
    char b_[64]; \
    size_t n_ = rp_text_normalize((in), b_, sizeof b_); \
    if (strcmp(b_, (want)) != 0 || n_ != strlen(want)) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, b_, (want)); failures++; } \
} while (0)

int main(void) {
    char small[8];
    EXPECT("", "");
    EXPECT(NULL, "");
    EXPECT("one", "one");
    EXPECT("a\nb", "a\nb");
    EXPECT("a\r\nb\rc", "a\nb\nc");
    EXPECT("a  \t\nb \n", "a\nb");
    EXPECT("\n\n  \nfirst\n\nsecond\n\n\n", "first\n\nsecond");
    EXPECT("   \n  \n", "");
    EXPECT("  indented\nnext", "  indented\nnext");
    /* cap cuts, always NUL terminated */
    rp_text_normalize("abcdefghijkl", small, sizeof small);
    if (strcmp(small, "abcdefg") != 0) { printf("FAIL cap: [%s]\n", small); failures++; }
    if (failures == 0) printf("text_norm: all ok\n");
    return failures ? 1 : 0;
}
