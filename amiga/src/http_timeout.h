#ifndef RP_HTTP_TIMEOUT_H
#define RP_HTTP_TIMEOUT_H

/* Socket timeouts of the Amiga transport, in seconds. The UI blocks while a request runs, so a dead network must fail
 * fast: connect gets a short limit for every request, and a body-less GET (the seed, the code check) a short read limit.
 * Requests that carry a body (publish, upload) and FTP keep a long read limit, since the server may work before it answers. */
#define RP_HTTP_CONNECT_SEC 5
#define RP_HTTP_GET_READ_SEC 10
#define RP_HTTP_LONG_READ_SEC 30

/* Read and write timeout after connect: 10 s for a GET without body, 30 s otherwise (has_body nonzero, or FTP). */
static int rp_http_read_seconds(const char *method, int has_body) {
    const char *m = method ? method : "";
    int is_get = m[0] == 'G' && m[1] == 'E' && m[2] == 'T' && m[3] == '\0';
    return is_get && !has_body ? RP_HTTP_GET_READ_SEC : RP_HTTP_LONG_READ_SEC;
}

#endif
