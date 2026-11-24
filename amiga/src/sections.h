#ifndef RP_SECTIONS_H
#define RP_SECTIONS_H

#include "groups.h"

/* Which parts of the form a group sees (desktop visibleSections): the board list and the System field are X Innovations only; media embeds,
 * website Publish and Revoke need a release server; the Door repo button needs both. Aminet and Build are always shown. */
typedef struct {
    int boards, system, media, publish, door;
} rp_sections;

rp_sections rp_sections_for(const rp_group *g);

/* Back/Next page step (desktop wizard nextAvailable): the nearest page from cur in direction dir (+1 or -1) among n pages whose shown[i] is
 * non-zero (shown NULL: all pages), or -1 when there is none, which disables that button. */
int rp_page_step(int cur, int dir, const int *shown, int n);

#endif
