#ifndef RP_SECTIONS_H
#define RP_SECTIONS_H

#include "groups.h"

/* Which parts of the form a group sees (desktop visibleSections): the board list and the System field are X Innovations only; media embeds,
 * website Publish and Revoke need a release server; the Door repo button needs both. Aminet and Build are always shown. */
typedef struct {
    int boards, system, media, publish, door;
    int history;      /* historyEntry: the Category row (the server's history entry needs one) */
    int number;       /* releaseNumberText: the typed Release number row (a server counts releases itself) */
    int creators;     /* creatorsAutocomplete: member hits in the Creators line */
    int diz_line;     /* dizReleaseLine: the Diz line field and its counter (every renderer but placeholder) */
    int diz_preview;  /* dizPreviewOnly: the template status line (placeholder renderer) */
} rp_sections;

rp_sections rp_sections_for(const rp_group *g);

/* Back/Next page step (desktop wizard nextAvailable): the nearest page from cur in direction dir (+1 or -1) among n pages whose shown[i] is
 * non-zero (shown NULL: all pages), or -1 when there is none, which disables that button. */
int rp_page_step(int cur, int dir, const int *shown, int n);

#endif
