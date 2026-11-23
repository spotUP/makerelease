/* multipart/form-data builder; bytes match Node's undici FormData (what the
 * desktop client sends): "--B\r\nContent-Disposition: form-data; name=\"n\"
 * [; filename=\"f\"]\r\n[Content-Type: application/octet-stream\r\n]\r\n
 * data\r\n", closed by "--B--\r\n". Names and filenames escape \n \r " as
 * %0A %0D %22. Whole body lives in memory. */
#ifndef RP_MULTIPART_H
#define RP_MULTIPART_H

#include "strbuf.h"

typedef struct {
    strbuf body;          /* body.data / body.len after rp_mp_finish; free(body.data) */
    char boundary[72];
} rp_mp;

/* undici shape: "----formdata-undici-0" + 11 digits from seed (caller supplies randomness). */
void rp_mp_boundary(char *out, size_t cap, unsigned long seed);
void rp_mp_init(rp_mp *m, const char *boundary);
int rp_mp_field(rp_mp *m, const char *name, const char *value);
int rp_mp_file(rp_mp *m, const char *name, const char *filename, const char *data, size_t len);
/* Writes the closing line and the Content-Type header value. */
int rp_mp_finish(rp_mp *m, char *ctype, size_t cap);

#endif
