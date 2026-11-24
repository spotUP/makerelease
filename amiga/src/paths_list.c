#include "paths_list.h"

#include <string.h>

static int same_fold(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x = (char)(x - 'a' + 'A');
        if (y >= 'a' && y <= 'z') y = (char)(y - 'a' + 'A');
        if (x != y) return 0;
    }
    return *a == *b;
}

int rp_paths_add(rp_paths *p, const char *path) {
    char t[RP_PATH_CAP];
    size_t len, i;
    if (path == NULL) return 0;
    len = strlen(path);
    if (len == 0) return 0;
    if (len >= RP_PATH_CAP) return -1;
    memcpy(t, path, len + 1);
    if (len >= 2 && t[len - 1] == '/' && t[len - 2] != ':') t[len - 1] = '\0';
    for (i = 0; i < p->n; i++)
        if (same_fold(p->path[i], t)) return 0;
    if (p->n >= RP_PATHS_MAX) return -1;
    strcpy(p->path[p->n++], t);
    return 1;
}

void rp_paths_remove(rp_paths *p, size_t i) {
    if (i >= p->n) return;
    memmove(p->path[i], p->path[i + 1], (p->n - i - 1) * RP_PATH_CAP);
    p->n--;
}

void rp_paths_replace(rp_paths *p, size_t i, const char *path) {
    if (i >= p->n || strlen(path) >= RP_PATH_CAP) return;
    strcpy(p->path[i], path);
}

int rp_paths_find(const rp_paths *p, const char *path) {
    size_t i;
    for (i = 0; i < p->n; i++)
        if (strcmp(p->path[i], path) == 0) return (int)i;
    return -1;
}
