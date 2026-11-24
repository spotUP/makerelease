#include "archive_preview.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "naming.h"

#define PATH_MAX_RP 1024

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Last segment; an Amiga volume colon also ends a segment. */
static const char *base_of(const char *path) {
    const char *s = strrchr(path, '/');
    const char *c = strrchr(path, ':');
    if (c && (!s || c > s)) s = c;
    return s ? s + 1 : path;
}

static void join(const char *dir, const char *name, char *out, size_t cap) {
    size_t n = strlen(dir);
    int bare = n > 0 && (dir[n - 1] == '/' || dir[n - 1] == ':');
    snprintf(out, cap, "%s%s%s", dir, bare ? "" : "/", name);
}

static int add(char ***v, size_t *n, size_t *cap, const char *s) {
    char *copy;
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 32;
        char **p = realloc(*v, nc * sizeof(char *));
        if (!p) return -1;
        *v = p;
        *cap = nc;
    }
    copy = malloc(strlen(s) + 1);
    if (!copy) return -1;
    strcpy(copy, s);
    (*v)[(*n)++] = copy;
    return 0;
}

static int cmp_fold(const void *a, const void *b) {
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    for (; *x && *y; x++, y++) {
        char cx = *x, cy = *y;
        if (cx >= 'A' && cx <= 'Z') cx = (char)(cx - 'A' + 'a');
        if (cy >= 'A' && cy <= 'Z') cy = (char)(cy - 'A' + 'a');
        if (cx != cy) return (unsigned char)cx - (unsigned char)cy;
    }
    return (unsigned char)*x - (unsigned char)*y;
}

/* Sorted names of dir (no . and ..); NULL entries list on failure. */
static int read_names(const char *dir, char ***names, size_t *n) {
    DIR *d = opendir(dir);
    struct dirent *e;
    size_t cap = 0;
    *names = NULL;
    *n = 0;
    if (!d) return -1;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (add(names, n, &cap, e->d_name) != 0) {
            closedir(d);
            return -1;
        }
    }
    closedir(d);
    qsort(*names, *n, sizeof(char *), cmp_fold);
    return 0;
}

static void free_names(char **v, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) free(v[i]);
    free(v);
}

static int list_tree(const char *dir, const char *prefix, char ***out, size_t *n, size_t *cap) {
    char **names;
    size_t cnt, i;
    int rc = 0;
    if (read_names(dir, &names, &cnt) != 0) return -1;
    for (i = 0; i < cnt && rc == 0; i++) {
        char rel[PATH_MAX_RP], full[PATH_MAX_RP];
        snprintf(rel, sizeof(rel), "%s%s", prefix, names[i]);
        join(dir, names[i], full, sizeof(full));
        if (is_dir(full)) {
            char entry[PATH_MAX_RP + 2], sub[PATH_MAX_RP + 2];
            snprintf(entry, sizeof(entry), "%s/", rel);
            rc = add(out, n, cap, entry);
            if (rc == 0) {
                strcpy(sub, entry);
                rc = list_tree(full, sub, out, n, cap);
            }
        } else {
            rc = add(out, n, cap, rel);
        }
    }
    free_names(names, cnt);
    return rc;
}

/* Follows a folder holding nothing but one folder, down to the first folder with real content. */
static int innermost(const char *src, char *out, size_t cap) {
    char cur[PATH_MAX_RP];
    snprintf(cur, sizeof(cur), "%s", src);
    for (;;) {
        char **names, next[PATH_MAX_RP];
        size_t n;
        if (read_names(cur, &names, &n) != 0) return -1;
        if (n != 1) {
            free_names(names, n);
            break;
        }
        join(cur, names[0], next, sizeof(next));
        free_names(names, n);
        if (!is_dir(next)) break;
        snprintf(cur, sizeof(cur), "%s", next);
    }
    snprintf(out, cap, "%s", cur);
    return 0;
}

int rp_preview_archive(const char *release, const char *const *paths, size_t npaths, rp_preview *out,
                       char *err, size_t errcap) {
    size_t cap = 0, ncap = 0, i;
    char head[PATH_MAX_RP], inner[PATH_MAX_RP], rel_up[128];
    memset(out, 0, sizeof *out);
    snprintf(inner, sizeof(inner), "%s/", release);
    if (add(&out->entries, &out->nentries, &cap, inner) != 0) goto oom;
    snprintf(head, sizeof(head), "%sfile_id.diz", inner);
    if (add(&out->entries, &out->nentries, &cap, head) != 0) goto oom;
    snprintf(head, sizeof(head), "%s/%s.NFO", release, release);
    if (add(&out->entries, &out->nentries, &cap, head) != 0) goto oom;
    for (i = 0; i < npaths; i++) {
        const char *src = paths[i];
        char deepest[PATH_MAX_RP], prefix[PATH_MAX_RP], entry[PATH_MAX_RP];
        if (!is_dir(src)) {
            snprintf(entry, sizeof(entry), "%s%s", inner, base_of(src));
            if (add(&out->entries, &out->nentries, &cap, entry) != 0) goto oom;
            continue;
        }
        rp_upper(base_of(src), rel_up, sizeof(rel_up));
        {
            char want[128];
            rp_upper(release, want, sizeof(want));
            if (strcmp(rel_up, want) == 0) {
                snprintf(prefix, sizeof(prefix), "%s", inner);
            } else {
                snprintf(prefix, sizeof(prefix), "%s%s/", inner, base_of(src));
                if (add(&out->entries, &out->nentries, &cap, prefix) != 0) goto oom;
            }
        }
        if (list_tree(src, prefix, &out->entries, &out->nentries, &cap) != 0) {
            snprintf(err, errcap, "%s: could not be read", src);
            rp_preview_free(out);
            return -1;
        }
        if (innermost(src, deepest, sizeof(deepest)) != 0) {
            snprintf(err, errcap, "%s: could not be read", src);
            rp_preview_free(out);
            return -1;
        }
        if (strcmp(deepest, src) != 0) {
            rp_nested *p;
            if (out->nnested == ncap) {
                ncap = ncap ? ncap * 2 : 4;
                p = realloc(out->nested, ncap * sizeof *p);
                if (!p) goto oom;
                out->nested = p;
            }
            p = &out->nested[out->nnested];
            p->path = malloc(strlen(src) + 1);
            p->flatten_to = malloc(strlen(deepest) + 1);
            if (!p->path || !p->flatten_to) {
                free(p->path);
                free(p->flatten_to);
                goto oom;
            }
            strcpy(p->path, src);
            strcpy(p->flatten_to, deepest);
            out->nnested++;
        }
    }
    return 0;
oom:
    snprintf(err, errcap, "out of memory");
    rp_preview_free(out);
    return -1;
}

void rp_preview_free(rp_preview *p) {
    size_t i;
    for (i = 0; i < p->nentries; i++) free(p->entries[i]);
    free(p->entries);
    for (i = 0; i < p->nnested; i++) {
        free(p->nested[i].path);
        free(p->nested[i].flatten_to);
    }
    free(p->nested);
    memset(p, 0, sizeof *p);
}
