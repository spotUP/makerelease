#include "http_timeout.h"

#include <stdio.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    /* the seed and code check are body-less GETs: short */
    CHECK(rp_http_read_seconds("GET", 0) == RP_HTTP_GET_READ_SEC);
    /* publish and upload carry a body or are not GET: long */
    CHECK(rp_http_read_seconds("POST", 1) == RP_HTTP_LONG_READ_SEC);
    CHECK(rp_http_read_seconds("DELETE", 0) == RP_HTTP_LONG_READ_SEC);
    CHECK(rp_http_read_seconds("GET", 1) == RP_HTTP_LONG_READ_SEC);
    CHECK(rp_http_read_seconds("GETX", 0) == RP_HTTP_LONG_READ_SEC);
    CHECK(rp_http_read_seconds(NULL, 0) == RP_HTTP_LONG_READ_SEC);
    /* connect is shorter than any read limit */
    CHECK(RP_HTTP_CONNECT_SEC < RP_HTTP_GET_READ_SEC && RP_HTTP_GET_READ_SEC < RP_HTTP_LONG_READ_SEC);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_http_timeout: ok\n");
    return 0;
}
