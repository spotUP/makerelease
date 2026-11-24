#include "sections.h"

#include "online.h"

rp_sections rp_sections_for(const rp_group *g) {
    rp_sections s;
    int server = RP_ONLINE_HAS_SERVER(g) ? 1 : 0;
    s.boards = g->renderer == R_XINNOVATIONS;
    s.system = s.boards;
    s.media = server;
    s.publish = server;
    s.door = server && s.boards;
    s.history = server;
    s.number = !server;
    s.creators = server;
    s.diz_line = g->renderer != R_PLACEHOLDER;
    s.diz_preview = g->renderer == R_PLACEHOLDER;
    return s;
}

int rp_page_step(int cur, int dir, const int *shown, int n) {
    int i;
    for (i = cur + dir; i >= 0 && i < n; i += dir)
        if (shown == NULL || shown[i]) return i;
    return -1;
}
