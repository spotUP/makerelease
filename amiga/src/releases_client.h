/* Releases server client; mirrors app/src/main/releases-client.ts (URLs,
 * headers, status handling, error messages). Pure logic over rp_http_fn. */
#ifndef RP_RELEASES_CLIENT_H
#define RP_RELEASES_CLIENT_H

#include "http.h"
#include "json.h"

typedef struct {
    const char *base_url;   /* no trailing slash */
    const char *write_key;  /* "" = reads only; writes still send the (empty) header */
    rp_http_fn http;
    void *http_ctx;
} rp_rel_cfg;

typedef struct {
    char message[256];      /* TS ReleasesApiError.message */
    int status;             /* 0 when unknown (transport failure) */
    char reason[48];        /* 409 reason, publish only, "" when absent */
    char wordpress_url[256];/* 409 publish only, "" when absent */
} rp_rel_err;

/* Each returns 0 and, where there is a body, a parsed JSON tree in *out (the
 * caller json_free()s it; {} when the body was not JSON), or -1 with err
 * filled. division: "up-rough" etc, percent-encoded here. */
int rp_rel_seed(const rp_rel_cfg *c, const char *division, jnode **out, rp_rel_err *err);
int rp_rel_manifest(const rp_rel_cfg *c, const char *division, jnode **out, rp_rel_err *err);
int rp_rel_numbering_check(const rp_rel_cfg *c, jnode **out, rp_rel_err *err);
/* *exists = server's "exists" boolean. */
int rp_rel_exists(const rp_rel_cfg *c, const char *code, int *exists, rp_rel_err *err);
/* body: the finished JSON publish body (the caller builds it, like toPublishBody). */
int rp_rel_create(const rp_rel_cfg *c, const char *json_body, jnode **out, rp_rel_err *err);
/* body: a finished multipart body with its content type, e.g.
 * "multipart/form-data; boundary=..."; the multipart builder is not written. */
int rp_rel_publish(const rp_rel_cfg *c, const char *code, const char *content_type,
                   const char *body, size_t body_len, jnode **out, rp_rel_err *err);
int rp_rel_delete(const rp_rel_cfg *c, const char *code, jnode **out, rp_rel_err *err);

/* encodeURIComponent: leaves A-Za-z0-9 - _ . ! ~ * ' ( ). Returns 0, or -1 when cap is too small. */
int rp_url_encode(const char *in, char *out, size_t cap);

/* ---- cfg filler: clientConfigFor ---- */
/* Profile values come from the UI's config struct; env_* are the optional
 * overrides (RELEASES_API_BASE_URL / RELEASES_WRITE_KEY; NULL or "" = unset,
 * which wins like the TS `||`). profile_base NULL = profile has no server
 * block: returns -1 (TS throws). out->base_url / write_key point at the
 * given strings; they must outlive the cfg. */
int rp_rel_cfg_for(rp_rel_cfg *out, const char *profile_base, const char *profile_key,
                   const char *env_base, const char *env_key, rp_http_fn http, void *http_ctx);

/* ---- toPublishBody / trimDizBanner ---- */
typedef struct { const char *nickname, *role; } rp_rel_member;
typedef struct { const char *role, *name; } rp_rel_credit;
typedef struct { const char *name, *sysop, *hq, *status; } rp_rel_board;
/* type "youtube" uses youtube_url; "image" ignores it. */
typedef struct { const char *type, *caption, *youtube_url; } rp_rel_embed;

typedef struct {
    const char *code, *format, *release_name, *platform_tag, *release_notes, *news, *diz_banner;
    const char *entry_title, *entry_category, *entry_creators;
    const char *division;            /* NULL = key omitted */
    const rp_rel_board *boards;      /* NULL = key omitted; n_boards may be 0 with non-NULL */
    size_t n_boards;
    const rp_rel_member *members; size_t n_members;
    const char *description;
    const rp_rel_credit *credits; size_t n_credits;
    const rp_rel_embed *embeds; size_t n_embeds;
} rp_rel_form;

/* Trailing blanks per line and blank lines at both ends removed; malloc'd, caller frees. NULL on OOM. */
char *rp_trim_diz(const char *text);
/* Compact JSON exactly as JSON.stringify(toPublishBody(form)); malloc'd, caller frees. NULL on OOM. */
char *rp_rel_publish_body(const rp_rel_form *f);

/* ---- publishToWeb: multipart parts archive, image, embedImage* ---- */
typedef struct { const char *filename; const char *data; size_t len; } rp_rel_file;
int rp_rel_publish_files(const rp_rel_cfg *c, const char *code, unsigned long boundary_seed,
                         const rp_rel_file *archive, const rp_rel_file *image,
                         const rp_rel_file *embed_images, size_t n_embeds,
                         jnode **out, rp_rel_err *err);

#endif
