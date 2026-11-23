#include "door_repo_client.h"
#include "json.h"
#include "multipart.h"
#include <stdio.h>
#include <stdlib.h>

static void say(rp_door_result *r, const char *a, const char *b) {
    snprintf(r->message, sizeof r->message, "%s%s", a, b);
}

int rp_door_submit(rp_http_fn http, void *ctx, const char *url, const char *filename,
                   const char *data, size_t len, unsigned long seed, rp_door_result *res) {
    rp_mp mp;
    char boundary[72], ctype[128], perr[64];
    rp_http_header h;
    rp_http_req req;
    rp_http_resp resp;
    jnode *j;
    const jnode *v;

    memset(res, 0, sizeof *res);
    res->id = -1;
    memset(&resp, 0, sizeof resp);
    rp_mp_boundary(boundary, sizeof boundary, seed);
    rp_mp_init(&mp, boundary);
    if (rp_mp_file(&mp, "file", filename, data, len) || rp_mp_finish(&mp, ctype, sizeof ctype)) {
        free(mp.body.data);
        say(res, "could not reach the door repository: out of memory", "");
        return -1;
    }
    h.name = "Content-Type"; h.value = ctype;
    req.method = "POST"; req.url = url ? url : RP_DOOR_REPO_SUBMISSIONS_URL;
    req.headers = &h; req.nheaders = 1; req.body = mp.body.data; req.body_len = mp.body.len;
    if (http(ctx, &req, &resp) != 0) {
        free(mp.body.data); free(resp.body);
        say(res, "could not reach the door repository: ", resp.error);
        return -1;
    }
    free(mp.body.data);
    j = resp.body ? json_parse(resp.body, perr, sizeof perr) : NULL;
    free(resp.body);
    if (resp.status < 200 || resp.status > 299) {
        v = j ? json_get(j, "error") : NULL;
        if (v && v->type == J_STR) say(res, v->str, "");
        else snprintf(res->message, sizeof res->message, "the door repository returned %d", resp.status);
        json_free(j);
        return -1;
    }
    v = j ? json_get(j, "message") : NULL;
    say(res, v && v->type == J_STR ? v->str : "submitted to the door repository", "");
    v = j ? json_get(j, "id") : NULL;
    if (v && v->type == J_NUM) res->id = v->num;
    res->ok = 1;
    json_free(j);
    return 0;
}
