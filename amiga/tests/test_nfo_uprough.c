#include "nfo_uprough.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    char *buf;
    size_t n;
    long len;
    if (!f) {
        printf("FAIL: cannot open %s\n", path);
        failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)(len < 0 ? 0 : len) + 1);
    n = len > 0 ? fread(buf, 1, (size_t)len, f) : 0;
    fclose(f);
    buf[n] = '\0';
    if (len_out) *len_out = n;
    return buf;
}

static void test_golden(void) {
    rp_member members[2] = { { "Spot", "code" }, { "Hoffman", "music" } };
    rp_nfo_form form = { "Hypocrisy", "A500", "gold1", 1, "2025-10-18",
                         "A demo for the Amiga 500 with a hard drive.\n\nReleased at Computer Space 2025.",
                         "Two new members joined this year.", members, 2 };
    rp_history hist[3] = {
        { "218", "up-gold1.lha", "Hypocrisy", "demo", "Spot, Hoffman" },
        { "217", "up-evoke.lha", "Evoke invitation", "intro",
          "Spot, Hoffman, Lazy, Zalo, Rasmus, Mixer, Juice, Pinchy, Dr Doom, Antichrist and everyone else at the party place" },
        { "216", "up-md15.lha", "More Doors Cursor Menu", "tool", "Zalo" },
    };
    char err[256];
    char *banner = read_file("tests/fixtures/ur-banner.txt", NULL);
    char *nfo = NULL, *diz = NULL;
    char *gold_nfo;
    size_t gold_nfo_len, gold_diz_len;
    char *gold_diz;
    CHECK(banner != NULL);
    if (!banner) return;
    nfo = rp_render_nfo_uprough("UP-", hist, 3, &form, err, sizeof(err));
    CHECK(nfo != NULL);
    diz = rp_render_diz(banner, err, sizeof(err));
    CHECK(diz != NULL);
    gold_nfo = read_file("tests/fixtures/UP-GOLD.NFO", &gold_nfo_len);
    gold_diz = read_file("tests/fixtures/UP-GOLD.DIZ", &gold_diz_len);
    if (nfo && gold_nfo) {
        if (strlen(nfo) != gold_nfo_len || memcmp(nfo, gold_nfo, gold_nfo_len) != 0) {
            printf("FAIL: NFO differs (got %lu bytes, want %lu)\n", (unsigned long)strlen(nfo),
                   (unsigned long)gold_nfo_len);
            failures++;
        }
    }
    if (diz && gold_diz) {
        if (strlen(diz) != gold_diz_len || memcmp(diz, gold_diz, gold_diz_len) != 0) {
            printf("FAIL: DIZ differs (got %lu bytes, want %lu)\n", (unsigned long)strlen(diz),
                   (unsigned long)gold_diz_len);
            failures++;
        }
    }
    free(banner);
    free(nfo);
    free(diz);
    free(gold_nfo);
    free(gold_diz);
}

int main(void) {
    test_golden();
    if (failures == 0) puts("nfo_uprough: golden files match");
    return failures != 0;
}
