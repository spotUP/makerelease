#include "release_seed.h"

#include <stdio.h>
#include <string.h>

#include "credits.h"
#include "naming.h"
#include "stale_drafts.h"

const char *const rp_release_categories[RP_RELEASE_CATEGORY_COUNT + 1] = {
    "Demo", "Intro", "Musicdisk", "Music", "Graphics", "ASCII", "Tool", "Door", "Video", "Science", "Other", NULL
};

int rp_release_category_index(const char *name) {
    int i;
    for (i = 0; rp_release_categories[i]; i++)
        if (strcmp(rp_release_categories[i], name) == 0) return i;
    return -1;
}

static const char *str_of(const jnode *o, const char *key) {
    const jnode *v = json_get(o, key);
    return v && v->type == J_STR ? v->str : "";
}

static void put(char *dst, size_t cap, const char *s) {
    snprintf(dst, cap, "%s", s);
}

int rp_seed_parse(const jnode *seed, const jnode *numbering, long now_seconds,
                  const char *profile_id, int has_write_key, rp_seed_info *out) {
    const jnode *v, *arr;
    size_t i;
    if (!seed || seed->type != J_OBJ) return -1;
    memset(out, 0, sizeof *out);

    v = json_get(seed, "nextReleaseNumber");
    out->next_number = v && v->type == J_NUM ? v->num : -1;
    if (out->next_number >= 0) snprintf(out->number_text, sizeof out->number_text, "%ld", out->next_number);
    out->current_diz = str_of(seed, "currentDizText");

    arr = json_get(seed, "members");
    if (arr && arr->type == J_ARR) {
        for (i = 0; i < arr->n && out->nmembers < RP_MEMBERS_MAX; i++) {
            rp_member *m = &out->members[out->nmembers++];
            put(m->nickname, sizeof m->nickname, str_of(arr->items[i], "nickname"));
            put(m->role, sizeof m->role, str_of(arr->items[i], "role"));
        }
        out->nmembers = rp_merge_member_skills(out->members, out->nmembers);
    }

    arr = json_get(seed, "boards");
    if (arr && arr->type == J_ARR) {
        for (i = 0; i < arr->n && out->nboards < RP_SEED_BOARDS_MAX; i++) {
            rp_rel_board *b = &out->boards[out->nboards++];
            b->name = str_of(arr->items[i], "name");
            b->sysop = str_of(arr->items[i], "sysop");
            b->hq = str_of(arr->items[i], "hq");
            b->status = str_of(arr->items[i], "status");
        }
    }

    if (numbering) {
        const jnode *checked = json_get(numbering, "checked"), *ok = json_get(numbering, "ok");
        const char *summary = str_of(numbering, "summary");
        if (checked && checked->type == J_BOOL && checked->num && ok && ok->type == J_BOOL && !ok->num && summary[0])
            snprintf(out->numbering_warning, sizeof out->numbering_warning,
                     "%s Do not build until this is reconciled - the release number goes into the NFO.", summary);
    }
    rp_describe_stale_drafts(json_get(seed, "drafts"), now_seconds, out->stale_warning, sizeof out->stale_warning);
    if (!has_write_key)
        snprintf(out->config_warning, sizeof out->config_warning,
                 "The \"%s\" profile has no server write key - publishing a release will fail. "
                 "Put the key in its profile.json and restart the packer.", profile_id ? profile_id : "");

    {
        const char *parts[3];
        size_t n = 0, len = 0;
        parts[n] = out->numbering_warning; if (parts[n][0]) n++;
        parts[n] = out->stale_warning; if (parts[n][0]) n++;
        parts[n] = out->config_warning; if (parts[n][0]) n++;
        for (i = 0; i < n; i++) {
            int w = snprintf(out->warnings + len, sizeof out->warnings - len, "%s%s", i ? " " : "", parts[i]);
            if (w < 0) break;
            len += (size_t)w >= sizeof out->warnings - len ? sizeof out->warnings - len - 1 : (size_t)w;
        }
    }
    return 0;
}

int rp_code_check(const rp_rel_cfg *c, const char *code, const char *base_name, char *msg, size_t cap) {
    int exists = 0;
    rp_rel_err e;
    msg[0] = 0;
    if (rp_rel_exists(c, code, &exists, &e) != 0) {
        snprintf(msg, cap, "%s", e.message);
        return -1;
    }
    if (exists) snprintf(msg, cap, "%s already exists - building will fail unless you change the code.", base_name);
    return exists ? 1 : 0;
}

int rp_release_number_preview(const char *number_text, const char *file_prefix, const char *code, int code_length, int lha,
                              char *out, size_t cap) {
    char xs[40], name[96], lower[96];
    size_t i;
    int n;
    if (code[0] == '\0') {
        for (i = 0; i < sizeof xs - 1 && (int)i < code_length; i++) xs[i] = 'X';
        xs[i] = '\0';
        code = xs;
    }
    rp_archive_name(file_prefix, code, lha, name, sizeof name);
    for (i = 0; (lower[i] = (char)((name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i])) != '\0'; i++) {}
    n = snprintf(out, cap, "release %s %s", number_text[0] ? number_text : "?", lower);
    return n >= 0 && (size_t)n < cap ? 0 : -1;
}

int rp_prefill_replaceable(const char *current, const char *last_prefill) {
    return current[0] == '\0' || strcmp(current, last_prefill) == 0;
}
