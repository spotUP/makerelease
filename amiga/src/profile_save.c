#include "profile_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "json_write.h"
#include "stage.h"

#define DIZ_FILE "file_id.diz"
#define NFO_FILE "release.nfo"
#define PROFILE_FILE "profile.json"

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int join(char *out, size_t cap, const char *a, const char *b, const char *c, char *err, size_t errcap) {
    int n = c ? snprintf(out, cap, "%s/%s/%s", a, b, c) : snprintf(out, cap, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= cap) {
        snprintf(err, errcap, "%s: the path is too long", a);
        return -1;
    }
    return 0;
}

static int oom(char *err, size_t errcap) {
    snprintf(err, errcap, "out of memory");
    return -1;
}

/* A field that fills its whole array has lost its terminator: refuse, never truncate. */
static int too_long(const char *field, const char *buf, size_t cap, char *err, size_t errcap) {
    if (memchr(buf, '\0', cap)) return 0;
    snprintf(err, errcap, "\"%s\" is too long (max %lu)", field, (unsigned long)(cap - 1));
    return -1;
}

static int put_field(strbuf *b, const char *indent, const char *key, const char *val, int first) {
    return sb_puts(b, first ? "\n" : ",\n") || sb_puts(b, indent) || jw_str(b, key) || sb_puts(b, ": ") || jw_str(b, val);
}

int rp_group_serialize(const rp_group *g, char *out, size_t cap, char *err, size_t errcap) {
    static const char *const renderers[] = {"uprough", "xinnovations", "placeholder"};
    strbuf b = {0};
    char num[16];
    size_t i;
    int bad = 0;

    if (too_long("label", g->label, sizeof g->label, err, errcap) || too_long("url", g->url, sizeof g->url, err, errcap) ||
        too_long("filePrefix", g->file_prefix, sizeof g->file_prefix, err, errcap) ||
        too_long("server.baseUrl", g->server_base, sizeof g->server_base, err, errcap) ||
        too_long("server.writeKey", g->server_key, sizeof g->server_key, err, errcap))
        return -1;
    if (g->nmembers > RP_MEMBERS_MAX) {
        snprintf(err, errcap, "\"members\" holds too many (max %d)", RP_MEMBERS_MAX);
        return -1;
    }
    for (i = 0; i < g->nmembers; i++) {
        char field[32];
        snprintf(field, sizeof field, "members[%lu].nickname", (unsigned long)i);
        if (too_long(field, g->members[i].nickname, sizeof g->members[i].nickname, err, errcap)) return -1;
        snprintf(field, sizeof field, "members[%lu].role", (unsigned long)i);
        if (too_long(field, g->members[i].role, sizeof g->members[i].role, err, errcap)) return -1;
    }
    if ((int)g->renderer < 0 || (int)g->renderer > 2) {
        snprintf(err, errcap, "\"renderer\" must be one of \"uprough\", \"xinnovations\", \"placeholder\"");
        return -1;
    }

    snprintf(num, sizeof num, "%d", g->code_length);
    bad |= sb_puts(&b, "{");
    bad |= put_field(&b, "  ", "label", g->label, 1);
    bad |= put_field(&b, "  ", "url", g->url, 0);
    bad |= put_field(&b, "  ", "filePrefix", g->file_prefix, 0);
    bad |= sb_puts(&b, ",\n  \"codeLength\": ") || sb_puts(&b, num);
    bad |= put_field(&b, "  ", "renderer", renderers[g->renderer], 0);
    bad |= put_field(&b, "  ", "backend", g->backend_doorserver ? "releases-doorserver" : "none", 0);
    if (g->backend_doorserver) {
        bad |= sb_puts(&b, ",\n  \"server\": {");
        bad |= put_field(&b, "    ", "baseUrl", g->server_base, 1);
        bad |= put_field(&b, "    ", "writeKey", g->server_key, 0);
        bad |= sb_puts(&b, "\n  }");
    }
    bad |= sb_puts(&b, ",\n  \"members\": [");
    for (i = 0; i < g->nmembers; i++) {
        bad |= sb_puts(&b, i ? ",\n    {" : "\n    {");
        bad |= put_field(&b, "      ", "nickname", g->members[i].nickname, 1);
        bad |= put_field(&b, "      ", "role", g->members[i].role, 0);
        bad |= sb_puts(&b, "\n    }");
    }
    bad |= sb_puts(&b, g->nmembers ? "\n  ]\n}\n" : "]\n}\n");
    if (bad) {
        free(b.data);
        return oom(err, errcap);
    }
    if (b.len + 1 > cap) {
        snprintf(err, errcap, "profile.json needs %lu bytes, the buffer holds %lu", (unsigned long)(b.len + 1),
                 (unsigned long)cap);
        free(b.data);
        return -1;
    }
    memcpy(out, b.data, b.len + 1);
    free(b.data);
    return 0;
}

static int write_text(const char *path, const char *text, char *err, size_t errcap) {
    return rp_write_file(path, text, strlen(text), err, errcap);
}

/* profile.json can hold server.writeKey: replaced atomically via a .tmp file. */
static int write_profile(const char *file, const char *text, char *err, size_t errcap) {
    char tmp[560];
    snprintf(tmp, sizeof tmp, "%s.tmp", file);
    if (write_text(tmp, text, err, errcap)) return -1;
    if (rename(tmp, file) != 0) {
        /* AmigaDOS Rename does not replace an existing target. */
        remove(file);
        if (rename(tmp, file) != 0) {
            remove(tmp);
            snprintf(err, errcap, "%s: could not be replaced", file);
            return -1;
        }
    }
    return 0;
}

/* Heap work area (text up to 64 members with full escaping, plus the parsed copy): stack is scarce on the Amiga. */
typedef struct {
    char text[40000];
    rp_group parsed;
} save_work;

static int save_checked(const char *dir, const rp_group *g, int is_new, const char *diz_or_null, const char *nfo_or_null,
                        int remove_nfo, save_work *w, char *err, size_t errcap);

int rp_group_save(const char *dir, const rp_group *g, int is_new, const char *diz_or_null, const char *nfo_or_null,
                  int remove_nfo, char *err, size_t errcap) {
    save_work *w = malloc(sizeof *w);
    int rc;
    if (!w) return oom(err, errcap);
    rc = save_checked(dir, g, is_new, diz_or_null, nfo_or_null, remove_nfo, w, err, errcap);
    free(w);
    return rc;
}

static int save_checked(const char *dir, const rp_group *g, int is_new, const char *diz_or_null, const char *nfo_or_null,
                        int remove_nfo, save_work *w, char *err, size_t errcap) {
    char folder[512], file[560], path[560], check_err[256];
    char *text = w->text;
    rp_group *parsed = &w->parsed;

    if (!rp_is_group_id(g->id)) {
        snprintf(err, errcap, "the drawer name \"%s\" must use only a-z, 0-9 and \"-\"", g->id);
        return -1;
    }
    if (join(folder, sizeof folder, dir, g->id, NULL, err, errcap)) return -1;
    if (is_new && is_dir(folder)) {
        snprintf(err, errcap, "A group with id %s already exists", g->id);
        return -1;
    }
    if (!is_new && !is_dir(folder)) {
        snprintf(err, errcap, "No group with id %s exists", g->id);
        return -1;
    }
    if (rp_group_serialize(g, text, sizeof w->text, err, errcap)) return -1;
    snprintf(file, sizeof file, "%s/" PROFILE_FILE, folder);
    if (rp_parse_group(text, file, g->id, parsed, check_err, sizeof check_err)) {
        snprintf(err, errcap, "%s", check_err);
        return -1;
    }
    if (is_new && rp_make_dirs(folder, err, errcap)) return -1;
    if (write_profile(file, text, err, errcap)) goto fail;
    if (diz_or_null) {
        snprintf(path, sizeof path, "%s/" DIZ_FILE, folder);
        if (write_text(path, diz_or_null, err, errcap)) goto fail;
    }
    snprintf(path, sizeof path, "%s/" NFO_FILE, folder);
    /* the NFO template is optional: an empty text writes no file (never a 0-byte release.nfo) */
    if (nfo_or_null && nfo_or_null[0] != '\0') {
        if (write_text(path, nfo_or_null, err, errcap)) goto fail;
    } else if (remove_nfo) {
        remove(path);
    }
    return 0;
fail:
    if (is_new) {
        char ignore[64];
        rp_remove_tree(folder, ignore, sizeof ignore);
    }
    return -1;
}

int rp_group_delete(const char *dir, const char *id, char *err, size_t errcap) {
    char folder[512], ids[2][RP_ID_MAX + 1];
    int n;

    if (!rp_is_group_id(id)) {
        snprintf(err, errcap, "the drawer name \"%s\" must use only a-z, 0-9 and \"-\"", id);
        return -1;
    }
    if (join(folder, sizeof folder, dir, id, NULL, err, errcap)) return -1;
    if (!is_dir(folder)) {
        snprintf(err, errcap, "No group with id %s exists", id);
        return -1;
    }
    n = rp_list_groups(dir, ids, 2, err, errcap);
    if (n < 0) return -1;
    if (n <= 1) {
        snprintf(err, errcap, "The last group cannot be deleted");
        return -1;
    }
    return rp_remove_tree(folder, err, errcap);
}

int rp_group_duplicate(const char *dir, const char *from_id, const char *new_id, char *err, size_t errcap) {
    char from[512], to[512];

    if (!rp_is_group_id(from_id) || !rp_is_group_id(new_id)) {
        snprintf(err, errcap, "the drawer name \"%s\" must use only a-z, 0-9 and \"-\"",
                 rp_is_group_id(from_id) ? new_id : from_id);
        return -1;
    }
    if (join(from, sizeof from, dir, from_id, NULL, err, errcap) || join(to, sizeof to, dir, new_id, NULL, err, errcap))
        return -1;
    if (!is_dir(from)) {
        snprintf(err, errcap, "No group with id %s exists", from_id);
        return -1;
    }
    if (is_dir(to)) {
        snprintf(err, errcap, "A group with id %s already exists", new_id);
        return -1;
    }
    if (rp_copy_tree(from, to, err, errcap)) {
        char ignore[64];
        rp_remove_tree(to, ignore, sizeof ignore);
        return -1;
    }
    return 0;
}
