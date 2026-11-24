#include "stale_drafts.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
#define CHECK_STR(a, b) do { if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } } while (0)

static int run(const char *json, long now, char *out, size_t cap) {
    char err[64];
    jnode *j = json_parse(json, err, sizeof err);
    int r = rp_describe_stale_drafts(j, now, out, cap);
    json_free(j);
    return r;
}

int main(void) {
    char out[600];
    long now = 1000000;

    CHECK(rp_describe_stale_drafts(NULL, now, out, sizeof out) == 0 && out[0] == 0);
    CHECK(run("[]", now, out, sizeof out) == 0);
    /* a draft younger than a day is the build just made: no warning */
    CHECK(run("[{\"code\":\"ABC\",\"releaseNumber\":5,\"createdAt\":990000}]", now, out, sizeof out) == 0);
    /* exactly one day: stale, "yesterday" */
    CHECK(run("[{\"code\":\"ABC\",\"releaseNumber\":217,\"createdAt\":913600}]", now, out, sizeof out) == 1);
    CHECK_STR(out, "An earlier build was never published and never revoked: ABC (release 217, built yesterday). "
                   "Nobody can see it, but its release number is spent. Publish or revoke to get the number back.");
    /* several: sorted by release number, days wording from 2 up */
    CHECK(run("[{\"code\":\"B\",\"releaseNumber\":9,\"createdAt\":700000},"
              "{\"code\":\"A\",\"releaseNumber\":3,\"createdAt\":500000},"
              "{\"code\":\"N\",\"releaseNumber\":12,\"createdAt\":999999}]", now, out, sizeof out) == 1);
    CHECK_STR(out, "Earlier builds were never published and never revoked: A (release 3, built 5 days ago), "
                   "B (release 9, built 3 days ago). Nobody can see them, but their release numbers are spent. "
                   "Publish or revoke to get the numbers back.");
    /* truncation stays NUL-terminated */
    CHECK(run("[{\"code\":\"ABC\",\"releaseNumber\":217,\"createdAt\":1}]", now, out, 20) == 1);
    CHECK(strlen(out) == 19);

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_stale_drafts ok\n");
    return 0;
}
