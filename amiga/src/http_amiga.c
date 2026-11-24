/* Real transport behind rp_http_fn: bsdsocket.library for TCP/DNS, AmiSSL for
 * https (SNI + certificate verification against AMISSL:Certs). One request per
 * call, "Connection: close"; the whole response is read to EOF, then parsed. */
#ifdef __amigaos__
#include "http.h"
#include "http_timeout.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

struct Library *SocketBase = NULL;
struct Library *AmiSSLMasterBase = NULL;
struct Library *AmiSSLBase = NULL;
struct Library *AmiSSLExtBase = NULL;

#define MAX_RESPONSE (8 * 1024 * 1024)

typedef struct { char *p; size_t len, cap; } rbuf;

static int rb_add(rbuf *b, const char *d, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 4096;
        char *np;
        while (nc < b->len + n + 1) nc *= 2;
        np = realloc(b->p, nc);
        if (!np) return -1;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, d, n);
    b->len += n;
    b->p[b->len] = 0;
    return 0;
}

static void fail(rp_http_resp *r, const char *fmt, const char *a)
{
    snprintf(r->error, sizeof r->error, fmt, a ? a : "");
}

/* scheme://host[:port]/path -> pieces; returns 0 ok. */
static int split_url(const char *url, int *tls, char *host, size_t hcap, int *port, const char **path)
{
    const char *p, *e;
    size_t n;
    if (!strncmp(url, "https://", 8)) { *tls = 1; *port = 443; p = url + 8; }
    else if (!strncmp(url, "http://", 7)) { *tls = 0; *port = 80; p = url + 7; }
    else return -1;
    e = p + strcspn(p, ":/?");
    n = (size_t)(e - p);
    if (n == 0 || n >= hcap) return -1;
    memcpy(host, p, n); host[n] = 0;
    if (*e == ':') { *port = atoi(e + 1); e += 1 + strspn(e + 1, "0123456789"); }
    if (*port <= 0 || *port > 65535) return -1;
    *path = (*e == '/' || *e == '?') ? e : "/";
    return 0;
}

/* read_sec: the read and write limit once connected; the connect itself gets RP_HTTP_CONNECT_SEC. */
static int connect_to(const char *host, int port, int read_sec, rp_http_resp *resp)
{
    struct hostent *he = gethostbyname((STRPTR)host);
    struct sockaddr_in sa;
    struct timeval tv;
    long on = 1, one = 1;
    int s;
    if (!he || he->h_addrtype != AF_INET || !he->h_addr_list[0]) {
        fail(resp, "DNS lookup failed for %s", host);
        return -1;
    }
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { fail(resp, "socket() failed%s", ""); return -1; }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], 4);
    tv.tv_secs = RP_HTTP_CONNECT_SEC; tv.tv_micro = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    (void)on; (void)one;
    if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) {
        char m[32];
        snprintf(m, sizeof m, "%s:%d", host, port);
        fail(resp, "connect to %s failed or timed out", m);
        CloseSocket(s);
        return -1;
    }
    tv.tv_secs = read_sec;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    return s;
}

static int send_all(int s, SSL *ssl, const char *d, size_t n)
{
    while (n) {
        int k = ssl ? SSL_write(ssl, d, (int)n) : (int)send(s, (APTR)d, (long)n, 0);
        if (k <= 0) return -1;
        d += k; n -= (size_t)k;
    }
    return 0;
}

static int hdr_eq(const char *line, const char *name, const char **val)
{
    size_t n = strlen(name);
    if (strncasecmp(line, name, n) || line[n] != ':') return 0;
    *val = line + n + 1;
    while (**val == ' ' || **val == '\t') (*val)++;
    return 1;
}

/* Parse the full response in raw; fills resp (status, body). Returns 0 ok. */
static int parse_response(rbuf *raw, rp_http_resp *resp)
{
    char *hdr_end = strstr(raw->p, "\r\n\r\n"), *line, *next;
    char *body;
    long clen = -1;
    int chunked = 0;
    rbuf out = {0};
    if (!hdr_end || strncmp(raw->p, "HTTP/", 5)) { fail(resp, "malformed HTTP response%s", ""); return -1; }
    *hdr_end = 0;
    body = hdr_end + 4;
    line = strchr(raw->p, ' ');
    resp->status = line ? atoi(line + 1) : 0;
    if (resp->status <= 0) { fail(resp, "malformed HTTP status line%s", ""); return -1; }
    for (line = strstr(raw->p, "\r\n"); line; line = next) {
        const char *v;
        line += 2;
        next = strstr(line, "\r\n");
        if (next) *next = 0;
        if (hdr_eq(line, "Content-Length", &v)) clen = atol(v);
        else if (hdr_eq(line, "Transfer-Encoding", &v) && !strncasecmp(v, "chunked", 7)) chunked = 1;
    }
    {
        size_t avail = raw->len - (size_t)(body - raw->p);
        if (chunked) {
            const char *p = body, *end = body + avail;
            for (;;) {
                char *e;
                unsigned long sz = strtoul(p, &e, 16);
                if (e == p) { free(out.p); fail(resp, "bad chunk size%s", ""); return -1; }
                p = strstr(e, "\r\n");
                if (!p) { free(out.p); fail(resp, "truncated chunked body%s", ""); return -1; }
                p += 2;
                if (sz == 0) break;
                if ((size_t)(end - p) < sz) { free(out.p); fail(resp, "truncated chunked body%s", ""); return -1; }
                if (rb_add(&out, p, sz)) { free(out.p); fail(resp, "out of memory%s", ""); return -1; }
                p += sz;
                if (end - p < 2) { free(out.p); fail(resp, "truncated chunked body%s", ""); return -1; }
                p += 2;
            }
            if (!out.p && rb_add(&out, "", 0)) { fail(resp, "out of memory%s", ""); return -1; }
            resp->body = out.p; resp->body_len = out.len;
        } else {
            size_t n = avail;
            if (clen >= 0) {
                if ((size_t)clen > avail) { fail(resp, "truncated body (Content-Length)%s", ""); return -1; }
                n = (size_t)clen;
            }
            if (rb_add(&out, body, n)) { free(out.p); fail(resp, "out of memory%s", ""); return -1; }
            resp->body = out.p; resp->body_len = out.len;
        }
    }
    return 0;
}

static void ssl_reason(rp_http_resp *resp, const char *what, SSL *ssl)
{
    unsigned long e = ERR_get_error();
    char eb[100] = "";
    long v = ssl ? SSL_get_verify_result(ssl) : X509_V_OK;
    if (e) ERR_error_string_n(e, eb, sizeof eb);
    if (v != X509_V_OK)
        snprintf(resp->error, sizeof resp->error, "%s: certificate %s", what, X509_verify_cert_error_string(v));
    else
        snprintf(resp->error, sizeof resp->error, "%s: %s", what, eb);
}

int rp_http_amiga(void *ctx, const rp_http_req *req, rp_http_resp *resp)
{
    char host[128];
    const char *path;
    int tls, port, s = -1, rc = -1, ssl_open = 0;
    rbuf head = {0}, raw = {0};
    SSL_CTX *sctx = NULL;
    SSL *ssl = NULL;
    size_t i;
    char tmp[2048];
    (void)ctx;

    resp->status = 0; resp->body = NULL; resp->body_len = 0; resp->error[0] = 0;
    if (split_url(req->url, &tls, host, sizeof host, &port, &path)) { fail(resp, "bad URL%s", ""); return -1; }

    if (!(SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4))) {
        fail(resp, "no TCP/IP stack (bsdsocket.library)%s", ""); return -1;
    }
    if (tls) {
        if (!(AmiSSLMasterBase = OpenLibrary((STRPTR)"amisslmaster.library", AMISSLMASTER_MIN_VERSION))) {
            fail(resp, "amisslmaster.library missing%s", ""); goto done;
        }
        long orc = OpenAmiSSLTags(AMISSL_CURRENT_VERSION, AmiSSL_UsesOpenSSLStructs, FALSE,
                           AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase, AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                           AmiSSL_SocketBase, (ULONG)SocketBase, AmiSSL_ErrNoPtr, (ULONG)&errno, TAG_DONE);
        if (orc != 0) {
            snprintf(resp->error, sizeof resp->error, "cannot open AmiSSL (error %ld, API version %d, master v%d)",
                     orc, (int)AMISSL_CURRENT_VERSION, (int)AmiSSLMasterBase->lib_Version);
            goto done;
        }
        ssl_open = 1;
    }

    /* request head */
    snprintf(tmp, sizeof tmp, "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", req->method, path, host);
    rb_add(&head, tmp, strlen(tmp));
    for (i = 0; i < req->nheaders; i++) {
        snprintf(tmp, sizeof tmp, "%s: %s\r\n", req->headers[i].name, req->headers[i].value);
        rb_add(&head, tmp, strlen(tmp));
    }
    if (req->body) {
        snprintf(tmp, sizeof tmp, "Content-Length: %lu\r\n", (unsigned long)req->body_len);
        rb_add(&head, tmp, strlen(tmp));
    }
    rb_add(&head, "\r\n", 2);

    if ((s = connect_to(host, port, rp_http_read_seconds(req->method, req->body != NULL), resp)) < 0) goto done;

    if (tls) {
        if (!(sctx = SSL_CTX_new(TLS_client_method()))) { ssl_reason(resp, "TLS context", NULL); goto done; }
        SSL_CTX_set_verify(sctx, SSL_VERIFY_PEER, NULL);
        if (SSL_CTX_load_verify_locations(sctx, NULL, "AMISSL:Certs") != 1 &&
            SSL_CTX_set_default_verify_paths(sctx) != 1) {
            ssl_reason(resp, "cannot load AMISSL:Certs", NULL); goto done;
        }
        if (!(ssl = SSL_new(sctx))) { ssl_reason(resp, "TLS session", NULL); goto done; }
        SSL_set_fd(ssl, s);
        SSL_set_tlsext_host_name(ssl, host);
        X509_VERIFY_PARAM_set1_host(SSL_get0_param(ssl), host, 0);
        if (SSL_connect(ssl) != 1) { ssl_reason(resp, "TLS handshake failed", ssl); goto done; }
    }

    if (send_all(s, ssl, head.p, head.len) ||
        (req->body && send_all(s, ssl, req->body, req->body_len))) {
        fail(resp, "send failed%s", ""); goto done;
    }
    for (;;) {
        int k = ssl ? SSL_read(ssl, tmp, sizeof tmp) : (int)recv(s, tmp, sizeof tmp, 0);
        if (k <= 0) {
            if (ssl && k < 0) { /* timeout or reset; a missing close_notify at EOF is tolerated below */
                if (!raw.len) { ssl_reason(resp, "TLS read failed", ssl); goto done; }
            } else if (!ssl && k < 0) { if (!raw.len) { fail(resp, "receive failed or timed out%s", ""); goto done; } }
            break;
        }
        if (raw.len + (size_t)k > MAX_RESPONSE || rb_add(&raw, tmp, (size_t)k)) {
            fail(resp, "response too large or out of memory%s", ""); goto done;
        }
    }
    if (!raw.len) { fail(resp, "empty response%s", ""); goto done; }
    rc = parse_response(&raw, resp);

done:
    if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
    if (sctx) SSL_CTX_free(sctx);
    if (s >= 0) CloseSocket(s);
    if (ssl_open) CloseAmiSSL();
    if (AmiSSLMasterBase) { CloseLibrary(AmiSSLMasterBase); AmiSSLMasterBase = NULL; }
    AmiSSLBase = AmiSSLExtBase = NULL;
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
    free(head.p); free(raw.p);
    if (rc && !resp->error[0]) fail(resp, "request failed%s", "");
    return rc ? -1 : 0;
}

/* Plain TCP for the Aminet FTP client (aminet.h). SocketBase is opened with the
 * first connection and closed with the last. */
#include "aminet.h"
static int tcp_open_count;
static int tcp_open(void *ctx, const char *host, int port, char *err, size_t errcap)
{
    rp_http_resp r;
    int s;
    (void)ctx;
    memset(&r, 0, sizeof r);
    if (!SocketBase && !(SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4))) {
        snprintf(err, errcap, "no TCP/IP stack (bsdsocket.library)"); return -1;
    }
    if ((s = connect_to(host, port, RP_HTTP_LONG_READ_SEC, &r)) < 0) {
        snprintf(err, errcap, "%s", r.error);
        if (!tcp_open_count) { CloseLibrary(SocketBase); SocketBase = NULL; }
        return -1;
    }
    tcp_open_count++;
    return s;
}
static int tcp_send(void *ctx, int h, const char *d, size_t n) { (void)ctx; return send_all(h, NULL, d, n); }
static int tcp_recv(void *ctx, int h, char *b, size_t cap) { (void)ctx; return (int)recv(h, (APTR)b, (long)cap, 0); }
static void tcp_close(void *ctx, int h)
{
    (void)ctx;
    CloseSocket(h);
    if (--tcp_open_count <= 0 && SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; tcp_open_count = 0; }
}
const rp_tcp_ops rp_tcp_amiga_ops = { tcp_open, tcp_send, tcp_recv, tcp_close };
#endif
