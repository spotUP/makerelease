#include "groups.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include "json.h"
#include "template.h"

static int is_id(const char *s) {
    size_t i, n = strlen(s);
    if (n == 0 || n > RP_ID_MAX) return 0;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
    }
    return 1;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int rp_list_groups(const char *dir, char ids[][RP_ID_MAX + 1], int max, char *err, size_t errcap) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (!d) {
        snprintf(err, errcap, "%s: could not be read", dir);
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        char full[512];
        if (e->d_name[0] == '.') continue;
        if (n >= max) break;
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (!is_dir(full)) continue;
        strncpy(ids[n], e->d_name, RP_ID_MAX);
        ids[n][RP_ID_MAX] = '\0';
        n++;
    }
    closedir(d);
    qsort(ids, (size_t)n, sizeof(ids[0]), cmp_str);
    return n;
}

static int get_text(const jnode *obj, const char *file, const char *key, const char *shown, int required, char *out,
                    size_t cap, char *err, size_t errcap) {
    const jnode *v = json_get(obj, key);
    if (!v) {
        if (!required) {
            out[0] = '\0';
            return 0;
        }
        snprintf(err, errcap, "%s: \"%s\" is missing", file, shown);
        return -1;
    }
    if (v->type != J_STR) {
        snprintf(err, errcap, "%s: \"%s\" must be a string", file, shown);
        return -1;
    }
    if (required && v->str[0] == '\0') {
        snprintf(err, errcap, "%s: \"%s\" must not be empty", file, shown);
        return -1;
    }
    if (strlen(v->str) > cap - 1) {
        snprintf(err, errcap, "%s: \"%s\" is too long (max %lu)", file, shown, (unsigned long)(cap - 1));
        return -1;
    }
    strcpy(out, v->str);
    return 0;
}

int rp_load_group(const char *dir, const char *id, rp_group *g, char *err, size_t errcap) {
    char file[512], *text = NULL;
    FILE *f;
    long len;
    size_t nread;
    jnode *root = NULL;
    const jnode *v, *server, *members;
    size_t i;
    int rc = -1;

    memset(g, 0, sizeof *g);
    if (!is_id(id)) {
        snprintf(err, errcap, "%s: the drawer name \"%s\" must use only a-z, 0-9 and \"-\"", dir, id);
        return -1;
    }
    strncpy(g->id, id, RP_ID_MAX);
    snprintf(file, sizeof(file), "%s/%s/profile.json", dir, id);
    f = fopen(file, "rb");
    if (!f) {
        snprintf(err, errcap, "%s: could not be read", file);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = malloc((size_t)(len < 0 ? 0 : len) + 1);
    if (!text) {
        fclose(f);
        snprintf(err, errcap, "%s: out of memory", file);
        return -1;
    }
    /* A UTF-8 BOM (Windows editors) would choke the parser; strip it. */
    nread = len > 0 ? fread(text, 1, (size_t)len, f) : 0;
    fclose(f);
    text[nread] = '\0';
    {
        char *t = text;
        if (nread >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) {
            t += 3;
            memmove(text, t, strlen(t) + 1);
        }
    }
    root = json_parse(text, err, errcap);
    free(text);
    if (!root) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s: is not valid JSON (%s)", file, err);
        strncpy(err, tmp, errcap - 1);
        err[errcap - 1] = '\0';
        return -1;
    }
    if (root->type != J_OBJ) {
        snprintf(err, errcap, "%s: must be a JSON object", file);
        goto done;
    }
    if (get_text(root, file, "label", "label", 1, g->label, sizeof(g->label), err, errcap)) goto done;
    if (get_text(root, file, "url", "url", 1, g->url, sizeof(g->url), err, errcap)) goto done;
    if (get_text(root, file, "filePrefix", "filePrefix", 0, g->file_prefix, sizeof(g->file_prefix), err, errcap)) goto done;
    for (i = 0; g->file_prefix[i]; i++) {
        char c = g->file_prefix[i];
        if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|') {
            snprintf(err, errcap, "%s: \"filePrefix\" must not contain spaces or any of \\ / : * ? \" < > |", file);
            goto done;
        }
    }
    g->code_length = 5;
    v = json_get(root, "codeLength");
    if (v) {
        if (v->type != J_NUM || v->num < 1 || v->num > 32) {
            snprintf(err, errcap, "%s: \"codeLength\" must be a whole number from 1 to 32", file);
            goto done;
        }
        g->code_length = (int)v->num;
    }
    v = json_get(root, "renderer");
    if (!v || v->type != J_STR) {
        snprintf(err, errcap, "%s: \"renderer\" must be one of \"uprough\", \"xinnovations\", \"placeholder\"", file);
        goto done;
    }
    if (strcmp(v->str, "uprough") == 0) g->renderer = R_UPROUGH;
    else if (strcmp(v->str, "xinnovations") == 0) g->renderer = R_XINNOVATIONS;
    else if (strcmp(v->str, "placeholder") == 0) g->renderer = R_PLACEHOLDER;
    else {
        snprintf(err, errcap, "%s: \"renderer\" must be one of \"uprough\", \"xinnovations\", \"placeholder\"", file);
        goto done;
    }
    v = json_get(root, "backend");
    if (v && v->type == J_STR && strcmp(v->str, "releases-doorserver") == 0) {
        if (g->renderer == R_PLACEHOLDER) {
            snprintf(err, errcap, "%s: \"backend\" can be \"releases-doorserver\" only with renderer \"uprough\" or \"xinnovations\"",
                     file);
            goto done;
        }
        server = json_get(root, "server");
        if (!server || server->type != J_OBJ) {
            snprintf(err, errcap, "%s: \"server\" is required when backend is \"releases-doorserver\"", file);
            goto done;
        }
        g->backend_doorserver = 1;
        if (get_text(server, file, "baseUrl", "server.baseUrl", 1, g->server_base, sizeof(g->server_base), err, errcap)) {
            goto done;
        }
        {
            size_t bl = strlen(g->server_base);
            while (bl > 0 && g->server_base[bl - 1] == '/') g->server_base[--bl] = '\0';
        }
        if (get_text(server, file, "writeKey", "server.writeKey", 0, g->server_key, sizeof(g->server_key), err, errcap)) {
            goto done;
        }
    } else if (v && v->type == J_STR && strcmp(v->str, "none") != 0) {
        snprintf(err, errcap, "%s: \"backend\" must be one of \"none\", \"releases-doorserver\"", file);
        goto done;
    } else if (v && v->type != J_STR) {
        snprintf(err, errcap, "%s: \"backend\" must be one of \"none\", \"releases-doorserver\"", file);
        goto done;
    }
    if (!g->backend_doorserver && json_get(root, "server")) {
        snprintf(err, errcap, "%s: \"server\" is allowed only when backend is \"releases-doorserver\"", file);
        goto done;
    }
    members = json_get(root, "members");
    if (members) {
        if (members->type != J_ARR) {
            snprintf(err, errcap, "%s: \"members\" must be a list", file);
            goto done;
        }
        if (members->n > RP_MEMBERS_MAX) {
            snprintf(err, errcap, "%s: \"members\" holds too many (max %d)", file, RP_MEMBERS_MAX);
            goto done;
        }
        for (i = 0; i < members->n; i++) {
            const jnode *m = members->items[i];
            const jnode *nn, *rr;
            if (!m || m->type != J_OBJ) {
                snprintf(err, errcap, "%s: \"members[%lu]\" must be an object with string \"nickname\" and \"role\"", file,
                         (unsigned long)i);
                goto done;
            }
            nn = json_get(m, "nickname");
            rr = json_get(m, "role");
            if (!nn || nn->type != J_STR || !rr || rr->type != J_STR) {
                snprintf(err, errcap, "%s: \"members[%lu]\" must be an object with string \"nickname\" and \"role\"", file,
                         (unsigned long)i);
                goto done;
            }
            if (strlen(nn->str) > RP_NICK_MAX || strlen(rr->str) > RP_ROLE_MAX) {
                snprintf(err, errcap, "%s: \"members[%lu]\" nickname/role too long (max %d/%d)", file, (unsigned long)i,
                         RP_NICK_MAX, RP_ROLE_MAX);
                goto done;
            }
            strcpy(g->members[i].nickname, nn->str);
            strcpy(g->members[i].role, rr->str);
        }
        g->nmembers = members->n;
    }
    rc = 0;
done:
    json_free(root);
    return rc;
}

int rp_read_template(const char *path, int optional, char **out, char *err, size_t errcap) {
    FILE *f = fopen(path, "rb");
    long len;
    size_t nread;
    *out = NULL;
    if (!f) {
        if (optional) return 0;
        snprintf(err, errcap, "%s: could not be read", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)(len < 0 ? 0 : len) + 1);
    if (!*out) {
        fclose(f);
        snprintf(err, errcap, "%s: out of memory", path);
        return -1;
    }
    nread = len > 0 ? fread(*out, 1, (size_t)len, f) : 0;
    fclose(f);
    (*out)[nread] = '\0';
    rp_normalise_line_endings(*out);
    return 0;
}
