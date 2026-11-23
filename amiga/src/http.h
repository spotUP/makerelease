/* Transport seam for the releases client. The client builds a request, a
 * transport function performs it and fills the response; host tests plug in
 * a fake. The real Amiga transport (bsdsocket + AmiSSL) implements rp_http_fn
 * as rp_http_amiga() below; it is not written yet. */
#ifndef RP_HTTP_H
#define RP_HTTP_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *value;
} rp_http_header;

typedef struct {
    const char *method;             /* "GET", "POST", "DELETE" */
    const char *url;                /* absolute, already percent-encoded */
    const rp_http_header *headers;  /* nheaders entries, may be NULL */
    size_t nheaders;
    const char *body;               /* NULL when none */
    size_t body_len;
} rp_http_req;

typedef struct {
    int status;        /* HTTP status; 0 until the transport fills it */
    char *body;        /* malloc'd, NUL-terminated; the caller frees; may stay NULL */
    size_t body_len;
    char error[160];   /* set by the transport on failure (no connection, TLS, ...) */
} rp_http_resp;

/* Returns 0 when a response arrived (any status), nonzero when the request
 * could not be performed; then resp->error says why. */
typedef int (*rp_http_fn)(void *ctx, const rp_http_req *req, rp_http_resp *resp);

#ifdef __amigaos__
/* Real transport, to be implemented: plain http:// over bsdsocket.library,
 * https:// over AmiSSL. ctx is unused. Same contract as rp_http_fn. */
int rp_http_amiga(void *ctx, const rp_http_req *req, rp_http_resp *resp);
#endif

#endif
