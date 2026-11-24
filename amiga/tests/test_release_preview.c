#include "release_preview.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } } while (0)

int main(void) {
    char big[256], small[24];
    size_t full, n;
    n = rp_release_preview_view("DIZ", "NFO", big, sizeof big);
    CHECK(strcmp(big, "---- file_id.diz ----\nDIZ\n\n---- release NFO ----\nNFO") == 0);
    CHECK(n == strlen(big));
    n = rp_release_preview_view("DIZ", NULL, big, sizeof big);
    CHECK(strstr(big, "no NFO template") != NULL && n == strlen(big));
    /* a small box cuts the text but reports the whole length, always NUL terminated */
    full = rp_release_preview_view("DIZ", "NFO", big, sizeof big);
    n = rp_release_preview_view("DIZ", "NFO", small, sizeof small);
    CHECK(n == full && n >= sizeof small && strlen(small) == sizeof small - 1);
    CHECK(strncmp(small, big, sizeof small - 1) == 0);
    if (failures == 0) printf("release_preview: all ok\n");
    return failures ? 1 : 0;
}
