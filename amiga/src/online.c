#include "online.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "door_repo_client.h"
#include "json.h"
#include "naming.h"
#include "releases_client.h"

int rp_latin1_to_utf8(const char *in, char *out, size_t cap) {
    size_t o = 0;
    if (cap == 0) return -1;
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        size_t need = c < 0x80 ? 1 : 2;
        if (o + need + 1 > cap) { out[o] = '\0'; return -1; }
        if (c < 0x80) out[o++] = (char)c;
        else { out[o++] = (char)(0xC0 | (c >> 6)); out[o++] = (char)(0x80 | (c & 0x3F)); }
    }
    out[o] = '\0';
    return 0;
}

/* malloc'd UTF-8 copy of a Latin-1 string; NULL on OOM. */
static char *u8dup(const char *s) {
    size_t cap = strlen(s) * 2 + 1;
    char *p = malloc(cap);
    if (p) rp_latin1_to_utf8(s, p, cap);
    return p;
}

static const char *base_of(const char *path) {
    const char *s = strrchr(path, '/'), *c = strrchr(path, ':');
    if (c && (!s || c > s)) s = c;
    return s ? s + 1 : path;
}

static char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) { buf[n] = '\0'; *len = (size_t)n; }
    return buf;
}

/* Division id and cfg from the group; -1 + msg when the group has no release server. */
static int group_cfg(const rp_group *g, rp_http_fn http, void *ctx, int need_key, rp_rel_cfg *cfg,
                     const char **division, char *msg, size_t cap) {
    if (g->renderer == R_UPROUGH) *division = "uprough";
    else if (g->renderer == R_XINNOVATIONS) *division = "xinnovations";
    else *division = NULL;
    if (!g->backend_doorserver || !*division ||
        rp_rel_cfg_for(cfg, g->server_base[0] ? g->server_base : NULL, g->server_key, NULL, NULL, http, ctx) != 0) {
        snprintf(msg, cap, "%s has no release server in its profile", g->label);
        return -1;
    }
    if (need_key && !cfg->write_key[0]) {
        snprintf(msg, cap, "The %s profile has no server write key - put it in profile.json", g->id);
        return -1;
    }
    return 0;
}

int rp_online_publish(const rp_online_publish_args *a, char *msg, size_t cap) {
    const rp_group *g = a->group;
    rp_rel_cfg cfg;
    const char *division;
    char *s[9 + 2 * RP_MEMBERS_MAX + 2 * RP_CREDITS_MAX + 4 * RP_ONLINE_BOARDS_MAX + 2 * RP_ONLINE_EMBEDS_MAX] = {0}, *archive = NULL, *image = NULL, *json = NULL;
    size_t i, ns = 0, alen = 0, ilen = 0, elen = 0;
    rp_rel_member mem[RP_MEMBERS_MAX];
    rp_rel_credit cr[RP_CREDITS_MAX];
    rp_rel_form f;
    rp_rel_file af, imf;
    rp_rel_board brd[RP_ONLINE_BOARDS_MAX];
    rp_rel_embed emb[RP_ONLINE_EMBEDS_MAX];
    rp_rel_file eimg[RP_ONLINE_EMBEDS_MAX];
    char *eimg_data[RP_ONLINE_EMBEDS_MAX] = {0};
    size_t nimg = 0, nmem;
    const rp_member *members;
    rp_rel_err err;
    jnode *out = NULL;
    int rc = -1;
    char *dizraw = NULL;
    size_t dlen = 0;

    if (group_cfg(g, a->http, a->http_ctx, 1, &cfg, &division, msg, cap) != 0) return -1;
    if (!a->category || !a->category[0]) { snprintf(msg, cap, "Not published - choose a category first"); return -1; }
    if (!a->image_path[0]) { snprintf(msg, cap, "Not published - pick a cover image first"); return -1; }
    archive = read_all(a->archive_path, &alen);
    if (!archive) { snprintf(msg, cap, "Not published - build the release first (%s is missing)", a->archive_path); goto done; }
    image = read_all(a->image_path, &ilen);
    if (!image) { snprintf(msg, cap, "Not published - %s could not be read", a->image_path); goto done; }
    dizraw = read_all(a->diz_path, &dlen);
    if (!dizraw) { snprintf(msg, cap, "Not published - build the release first (%s is missing)", a->diz_path); goto done; }

    memset(&f, 0, sizeof f);
    f.code = s[ns++] = u8dup(a->code);
    f.format = a->format;
    f.release_name = s[ns++] = u8dup(a->release_name);
    f.platform_tag = s[ns++] = u8dup(a->platform_tag);
    f.release_notes = s[ns++] = u8dup(a->release_notes ? a->release_notes : "");
    f.news = s[ns++] = u8dup(a->news ? a->news : "");
    f.diz_banner = s[ns++] = u8dup(dizraw);
    f.entry_title = f.release_name;
    f.entry_category = s[ns++] = u8dup(a->category);
    f.entry_creators = s[ns++] = u8dup(a->creators ? a->creators : "");
    f.division = division;
    f.description = s[ns++] = u8dup(a->description);
    members = a->members ? a->members : g->members;
    nmem = a->members ? a->n_members : g->nmembers;
    if (nmem > RP_MEMBERS_MAX || a->n_boards > RP_ONLINE_BOARDS_MAX || a->n_embeds > RP_ONLINE_EMBEDS_MAX) {
        snprintf(msg, cap, "Not published - too many members, boards or media embeds");
        goto done;
    }
    for (i = 0; i < nmem; i++) {
        mem[i].nickname = s[ns++] = u8dup(members[i].nickname);
        mem[i].role = s[ns++] = u8dup(members[i].role);
    }
    f.members = mem;
    f.n_members = nmem;
    /* No boards: leave the key out so the stored list carries forward (desktop: boards.length > 0 ? { boards } : {}). */
    for (i = 0; i < a->n_boards; i++) {
        brd[i].name = s[ns++] = u8dup(a->boards[i].name);
        brd[i].sysop = s[ns++] = u8dup(a->boards[i].sysop);
        brd[i].hq = s[ns++] = u8dup(a->boards[i].hq);
        brd[i].status = s[ns++] = u8dup(a->boards[i].status);
    }
    if (a->n_boards) { f.boards = brd; f.n_boards = a->n_boards; }
    for (i = 0; i < a->n_embeds; i++) {
        const rp_embed_row *e = &a->embeds[i];
        emb[i].type = e->is_image ? "image" : "youtube";
        emb[i].caption = s[ns++] = u8dup(e->caption);
        emb[i].youtube_url = e->is_image ? "" : (s[ns++] = u8dup(e->url));
        if (e->is_image) {
            eimg_data[nimg] = read_all(e->path, &elen);
            if (!eimg_data[nimg]) { snprintf(msg, cap, "Not published - media embed image %s could not be read", e->path); goto done; }
            eimg[nimg].filename = base_of(e->path); eimg[nimg].data = eimg_data[nimg]; eimg[nimg].len = elen;
            nimg++;
        }
    }
    f.embeds = emb;
    f.n_embeds = a->n_embeds;
    for (i = 0; i < a->n_credits && i < RP_CREDITS_MAX; i++) {
        cr[i].role = s[ns++] = u8dup(a->credits[i].role);
        cr[i].name = s[ns++] = u8dup(a->credits[i].name);
        f.n_credits = i + 1;
    }
    f.credits = cr;
    for (i = 0; i < ns; i++) if (!s[i]) { snprintf(msg, cap, "Not published - out of memory"); goto done; }

    json = rp_rel_publish_body(&f);
    if (!json) { snprintf(msg, cap, "Not published - out of memory"); goto done; }
    if (rp_rel_create(&cfg, json, &out, &err) != 0) { snprintf(msg, cap, "Not published - %s", err.message); goto done; }
    json_free(out);
    out = NULL;
    af.filename = base_of(a->archive_path); af.data = archive; af.len = alen;
    imf.filename = base_of(a->image_path); imf.data = image; imf.len = ilen;
    if (rp_rel_publish_files(&cfg, a->code, a->seed, &af, &imf, eimg, nimg, &out, &err) != 0) {
        if (err.status == 409 && err.wordpress_url[0]) snprintf(msg, cap, "Already published: %s", err.wordpress_url);
        else snprintf(msg, cap, "Not published - %s", err.message);
        goto done;
    }
    {
        const jnode *w = json_get(out, "wordpressPostUrl"), *ar = json_get(out, "archiveUrl");
        snprintf(msg, cap, "Published: %s (archive: %s)", w && w->type == J_STR ? w->str : "?",
                 ar && ar->type == J_STR ? ar->str : "?");
    }
    rc = 0;
done:
    json_free(out);
    for (i = 0; i < ns; i++) free(s[i]);
    for (i = 0; i < nimg; i++) free(eimg_data[i]);
    free(archive); free(image); free(dizraw); free(json);
    return rc;
}

int rp_online_seed(const rp_group *g, rp_http_fn http, void *http_ctx, long now_seconds, rp_seed_info *info,
                   jnode **tree, char *msg, size_t cap) {
    rp_rel_cfg cfg;
    const char *division;
    rp_rel_err err;
    jnode *seed = NULL, *num = NULL;
    if (group_cfg(g, http, http_ctx, 0, &cfg, &division, msg, cap) != 0) return -1;
    if (rp_rel_seed(&cfg, division, &seed, &err) != 0) { snprintf(msg, cap, "%s", err.message); return -1; }
    /* the numbering check is a warning, not a precondition: a failed call means no check */
    if (rp_rel_numbering_check(&cfg, &num, &err) != 0) num = NULL;
    if (rp_seed_parse(seed, num, now_seconds, g->id, g->server_key[0] != '\0', info) != 0) {
        snprintf(msg, cap, "The release server sent no seed data");
        json_free(seed);
        json_free(num);
        return -1;
    }
    json_free(num);
    *tree = seed;
    msg[0] = '\0';
    return 0;
}

int rp_online_code_check(const rp_group *g, const char *code, rp_http_fn http, void *http_ctx, char *msg, size_t cap) {
    rp_rel_cfg cfg;
    const char *division;
    char base[64], none[8];
    msg[0] = '\0';
    if (group_cfg(g, http, http_ctx, 0, &cfg, &division, none, sizeof none) != 0) return 0;
    rp_base_name(g->file_prefix, code, base, sizeof base);
    return rp_code_check(&cfg, code, base, msg, cap);
}

int rp_online_revoke(const rp_group *g, const char *code, rp_http_fn http, void *http_ctx, char *msg, size_t cap) {
    rp_rel_cfg cfg;
    const char *division;
    rp_rel_err err;
    jnode *out = NULL;
    const jnode *n, *w;
    long num = 0;
    if (!code[0]) { snprintf(msg, cap, "Enter the code of the release to revoke"); return -1; }
    if (group_cfg(g, http, http_ctx, 1, &cfg, &division, msg, cap) != 0) return -1;
    if (rp_rel_delete(&cfg, code, &out, &err) != 0) { snprintf(msg, cap, "Not revoked - %s", err.message); return -1; }
    n = json_get(out, "releaseNumber");
    if (n && n->type == J_NUM) num = n->num;
    w = json_get(out, "wasDraft");
    if (w && w->type == J_BOOL && w->num)
        snprintf(msg, cap, "Revoked release %ld. It was never published, so nothing was ever public.", num);
    else {
        const jnode *pe = json_get(out, "wordpressPostError"), *url = json_get(out, "wordpressPostUrl");
        const jnode *ok = json_get(out, "archiveRemoved");
        int bad_post = pe && pe->type == J_STR;
        snprintf(msg, cap, "Revoked release %ld.%s%s%s", num,
                 bad_post ? " Its WordPress post could NOT be trashed - remove it by hand: " : "",
                 bad_post && url && url->type == J_STR ? url->str : "",
                 ok && ok->type == J_BOOL && !ok->num ? " Its archive could not be removed - check the server logs." : "");
    }
    json_free(out);
    return 0;
}

int rp_online_door(const rp_group *g, const char *archive_path, unsigned long seed, rp_http_fn http,
                   void *http_ctx, char *msg, size_t cap) {
    rp_door_result r;
    size_t len = 0;
    char *data;
    int rc;
    if (g->renderer != R_XINNOVATIONS) { snprintf(msg, cap, "The door repository takes /X Innovations builds only"); return -1; }
    data = read_all(archive_path, &len);
    if (!data) { snprintf(msg, cap, "Build the release first (%s is missing)", archive_path); return -1; }
    rc = rp_door_submit(http, http_ctx, NULL, base_of(archive_path), data, len, seed, &r);
    free(data);
    snprintf(msg, cap, "%s%s", rc == 0 ? "Door repository: " : "Door repository refused: ", r.message);
    return rc;
}
