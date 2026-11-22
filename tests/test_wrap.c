#include "wrap.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_LINE(w, i, s) do { \
    if ((w).n <= (i) || strcmp((w).lines[i], (s)) != 0) { \
        printf("FAIL %d: line %d got [%s] expect [%s]\n", __LINE__, (i), (w).n > (i) ? (w).lines[i] : "<none>", (s)); \
        failures++; \
    } \
} while (0)

int main(void) {
    rp_lines w = rp_wrap_words("a bb ccc dddd", 5, 5);
    CHECK(w.n == 3);
    CHECK_LINE(w, 0, "a bb");
    CHECK_LINE(w, 1, "ccc");
    CHECK_LINE(w, 2, "dddd");
    rp_free_lines(&w);
    w = rp_wrap_words("  spaced\tout\n\n words  ", 10, 10);
    CHECK(w.n == 2);
    CHECK_LINE(w, 0, "spaced out");
    CHECK_LINE(w, 1, "words");
    rp_free_lines(&w);
    w = rp_wrap_words("", 10, 10);
    CHECK(w.n == 1);
    CHECK_LINE(w, 0, "");
    rp_free_lines(&w);
    w = rp_wrap_words("averylongwordthatdoesnotfit", 5, 5);
    CHECK(w.n == 1);
    CHECK_LINE(w, 0, "averylongwordthatdoesnotfit");
    rp_free_lines(&w);
    w = rp_wrap_words("Spot, Hoffman, Lazy and everyone else at the party place", 48, 48);
    {
        size_t i;
        for (i = 0; i < w.n; i++) CHECK(strlen(w.lines[i]) <= 48);
    }
    rp_free_lines(&w);
    if (failures == 0) puts("wrap: all passed");
    return failures != 0;
}
