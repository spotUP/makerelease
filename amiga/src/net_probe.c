/* net-probe URL: GET and print the status and the first 300 body bytes. Amiga only. */
#include <stdio.h>
#include <stdlib.h>
#include "http.h"

int main(int argc, char **argv)
{
    rp_http_req q = { "GET", NULL, NULL, 0, NULL, 0 };
    rp_http_resp r = {0};
    int rc, n;
    if (argc < 2) { printf("usage: net-probe URL\n"); return 5; }
    q.url = argv[1];
    rc = rp_http_amiga(NULL, &q, &r);
    if (rc) { printf("FAIL: %s\n", r.error); return 10; }
    printf("status %d, %lu body bytes\n", r.status, (unsigned long)r.body_len);
    n = r.body_len < 300 ? (int)r.body_len : 300;
    if (r.body) printf("%.*s\n", n, r.body);
    free(r.body);
    return 0;
}
