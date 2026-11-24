#ifndef RP_SECTIONS_H
#define RP_SECTIONS_H

#include "groups.h"

/* Which parts of the form a group sees (desktop visibleSections): the board list is X Innovations only; media embeds,
 * website Publish and Revoke need a release server; the Door repo button needs both. Aminet and Build are always shown. */
typedef struct {
    int boards, media, publish, door;
} rp_sections;

rp_sections rp_sections_for(const rp_group *g);

#endif
