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
    return s;
}
