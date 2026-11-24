#include "sections.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    rp_group g;
    rp_sections s;
    memset(&g, 0, sizeof g);
    g.renderer = R_UPROUGH;
    g.backend_doorserver = 1;
    s = rp_sections_for(&g);
    CHECK(!s.boards && !s.system && s.media && s.publish && !s.door);
    g.renderer = R_XINNOVATIONS;
    s = rp_sections_for(&g);
    CHECK(s.boards && s.system && s.media && s.publish && s.door);
    g.backend_doorserver = 0;
    s = rp_sections_for(&g);
    CHECK(s.boards && s.system && !s.media && !s.publish && !s.door);
    g.renderer = R_PLACEHOLDER;
    g.backend_doorserver = 1;
    s = rp_sections_for(&g);
    CHECK(!s.boards && !s.system && !s.media && !s.publish && !s.door);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_sections: ok\n");
    return 0;
}
