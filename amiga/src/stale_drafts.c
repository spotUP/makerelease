#include "stale_drafts.h"

#include <stdio.h>
#include <stdlib.h>

static long num_of(const jnode *o, const char *key) {
    const jnode *v = json_get(o, key);
    return v && v->type == J_NUM ? v->num : 0;
}

static void app(char *out, size_t cap, size_t *len, const char *fmt, const char *a, long n1, long n2) {
    int w;
    if (*len + 1 >= cap) return;
    w = snprintf(out + *len, cap - *len, fmt, a, n1, n2);
    if (w < 0) return;
    *len += (size_t)w >= cap - *len ? cap - *len - 1 : (size_t)w;
}

int rp_describe_stale_drafts(const jnode *drafts, long now_seconds, char *out, size_t cap) {
    const jnode **stale;
    size_t i, j, n = 0, len = 0;
    int one;
    if (cap == 0) return 0;
    out[0] = 0;
    if (!drafts || drafts->type != J_ARR || drafts->n == 0) return 0;
    stale = malloc(drafts->n * sizeof *stale);
    if (!stale) return 0;
    for (i = 0; i < drafts->n; i++) {
        const jnode *d = drafts->items[i], *c = json_get(d, "createdAt");
        if (c && c->type == J_NUM && now_seconds - c->num >= RP_STALE_DRAFT_AGE_SECONDS) {
            /* insertion sort by releaseNumber, stable */
            for (j = n; j > 0 && num_of(stale[j - 1], "releaseNumber") > num_of(d, "releaseNumber"); j--) stale[j] = stale[j - 1];
            stale[j] = d;
            n++;
        }
    }
    if (n == 0) { free(stale); return 0; }
    one = n == 1;
    app(out, cap, &len, "%s never published and never revoked: ", one ? "An earlier build was" : "Earlier builds were", 0, 0);
    for (i = 0; i < n; i++) {
        const jnode *code = json_get(stale[i], "code");
        long age = now_seconds - num_of(stale[i], "createdAt"), days = age / 86400;
        char when[24];
        if (days >= 2) snprintf(when, sizeof when, "%ld days ago", days);
        else snprintf(when, sizeof when, "yesterday");
        if (i) app(out, cap, &len, "%s", ", ", 0, 0);
        app(out, cap, &len, "%s (release ", code && code->type == J_STR ? code->str : "", 0, 0);
        app(out, cap, &len, "%s%ld, built ", "", num_of(stale[i], "releaseNumber"), 0);
        app(out, cap, &len, "%s)", when, 0, 0);
    }
    app(out, cap, &len, ". Nobody can see %s, but ", one ? "it" : "them", 0, 0);
    app(out, cap, &len, "%s spent. Publish or revoke to get ", one ? "its release number is" : "their release numbers are", 0, 0);
    app(out, cap, &len, "%s back.", one ? "the number" : "the numbers", 0, 0);
    free(stale);
    return 1;
}
