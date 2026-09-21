---
date: 2026-09-21
topic: the Up Rough /X Innovations division, end to end
tags: [makerelease, releases-doorserver, nfo, diz, x-innovations, door-repo]
status: implemented
---

# Up Rough /X Innovations, as a second division

Research: `thoughts/shared/research/2026-09-20_x-innovations-release-format.md`.
Plan: `thoughts/shared/plans/2026-09-20-x-innovations-division.md` - all six
phases done. Reference release: `~/Downloads/up-md15`, fixture at
`app/test/fixtures/UP-MD15.NFO`.

## State

Working end to end, with one gap named at the bottom.

- Both divisions live. Up Rough at 219, next 220. /X at oo7, next **oo8**.
- /X has its own roster (18 members in three groups) and five boards,
  backfilled from UP-MD15.NFO.
- Seven /X releases backfilled from the archives in `~/Downloads/releaes`,
  and **all seven now have WordPress posts**: covers rendered from their own
  FILE_ID.DIZ in Topaz, filed under Tools plus a new `/X Innovations`
  category, dated so the site lists them oo1 to oo7 in order.
- The generated FILE_ID.DIZ and NFO were checked by spot from the app and
  are right.
- The app is installed and verified; the three member zips on the Desktop
  are rebuilt from this code.
- Both repos pushed. Server deployed.

## The site backfill

`scripts/diz-cover.py` renders a FILE_ID.DIZ as a cover image: the DIZ is
already the artwork, drawn on an 80-column screen in the Amiga's own font, so
a cover is that art in Topaz - light grey on near-black at 3x, the colours
the release index uses. Covers are in `~/Desktop/x-innovations-covers/`.

`scripts/publish-historical.ts` gives a backfilled release its post. It is a
script rather than a route on purpose: `POST /:code/publish` is for a release
being made now and rightly insists on an archive to upload, while these seven
already existed, one has no surviving archive, and their posts had to carry
their own dates.

**Which dates are evidence and which are not.** oo1 (Jan 5), oo2 (Feb 7),
oo3 (Mar 3), oo4 (Mar 6) and oo7 (Jun 18) come from the newest file inside
each archive. **oo5 (Apr 14) and oo6 (May 12) are placeholders**: their own
file dates - Feb 5 and Jan 22 - would sort them before releases they came
after. Those two need somebody's memory, not the filesystem's; each is a
one-line change.

**oo4 was recovered.** `UP-DGC11.zip` turned up on 2026-09-21 and is a real
v1.1, not a duplicate of v1.0: the same file sizes, but a colour change and
a date bug fixed (`MM-DD-YY` to `DD-MM-YY`). The archive is on the site, the
post offers it, the catalogue names it, and its date is now evidence rather
than a guess. It carries no FILE_ID.DIZ, so its banner is still the one
recovered from UP-MD15's release list - the only record of how it was drawn.
The door repository already had the file, which its sha256 check said when
the submission was tried.

## What a division is

`app/src/shared/divisions.ts` holds everything that differs - label, banner
art, which rows of it a release may retype, whether it inherits the previous
release's art, whether it goes to the door repository. Nothing else asks
which division it is and branches.

Server side it is a `division` column with its own `MAX()+1`, and everything
that reads or writes the catalogue takes one, defaulting to `uprough` so a
client that never heard of divisions is unaffected.

## Things the fixture taught that nobody would have guessed

The /X NFO reproduces `UP-MD15.NFO` byte for byte across all 264 lines, with
one asserted exception: a 283-column line the source arrived damaged on. It
took five findings to get there, each measured off the file:

- The boards table sits on fixed columns - name at 2, sysop at 22, HQ ending
  at 36, status ending at 56 - and **the HQ column is right-aligned**.
- Notes have 58 columns, not 57. One too few wrapped a line that fitted in
  the original and grew the whole NFO by a row.
- There is a **group-news block** under its own drawn heading that nobody
  mentioned; the form already had the field.
- Release-list banners are **indented at a fixed column**, not centred per
  line - centring makes the logo's short rows drift.
- The group centres **left-heavy** on an odd remainder, everywhere.

The seven historical entries cannot all be reproduced by one rule: oo7 is
centred a column left of oo6, and oo1 is in title case from before the
lower-case lettering. The golden test feeds each entry the art the file
carries, which is what a real build does too.

## Corrections made along the way

- `insertHistoricalRelease` was division-blind and would have shoved all 219
  Up Rough releases up by one on the first /X insert.
- The /X DIZ was 12 rows; FILE_ID.DIZ allows ten. It is now ten, like Up
  Rough's, and the tagline row is gone.
- After that shortening, the form still inherited the previous release's
  12-row art, so the release name went into the wrong line and the list
  showed the PREVIOUS release's name. A division now says whether its art is
  inherited at all, and the name is found by the line announcing the
  division rather than by row number - which is what makes the 11, 12 and
  13-row historical banners readable.
- An empty section left a bare unframed line, tearing a gap through the
  drawing. An empty section now takes no row.
- Coder and Design were separate inputs duplicating the credits, the same
  mistake as Creator(s). Both are derived from the credit rows now.

## Next Steps

0. **Two dates are guesses.** oo5 and oo6, above. Everything else about the
   seven is sourced.
1. **A real /X release has still never been made.** Everything around it is
   exercised - the generator by a golden test and by spot's dry run, the
   WordPress pipeline by seven real posts - but no /X build has taken a
   number, packed an archive and submitted it to the door repository. oo8
   will be the first.
2. ~~The seven /X releases have no WordPress posts~~ **done**, see above.
3. oo4 (dD Global Last Callers v1.1) has the placeholder filename
   `up-dgc11.zip`: its archive is lost everywhere including the door
   repository, and its art came from the NFO's own release list.
4. The catalogue now disagrees with UP-MD15's typed list in four places
   (`cONF cOMMANDER 1.0` vs `cONFERENCE cOMMANDER v1.0`, three
   capitalisations). The archives won, because that is what shipped.

## Other Notes

- A /X build submits its archive to `doors.uprough.net` as any member would,
  into the queue a curator approves. No curation key is baked into the app:
  the builds members receive are unsigned zips anyone can unpack.
- Backups on hippoplayer: `releases.db.bak-2026-09-20-division`,
  `-rosters`, `-phase5`, `releases.db.bak-2026-09-21-xbackfill`.
- `hdiutil` failing a DMG build means a stale disk image is still attached
  from an earlier run - `hdiutil info`, then detach the orphan.
