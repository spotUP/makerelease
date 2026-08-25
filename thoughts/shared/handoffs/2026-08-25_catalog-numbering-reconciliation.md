---
date: 2026-08-25
topic: catalogue vs WordPress numbering drift, GERP 2026 insert, hypocrisy 216 -> 217
tags: [makerelease, releases-doorserver, wordpress, catalog, numbering]
status: implemented
---

# Catalogue numbering reconciled with the website

Varin0x could build the Evoke 2026 release but not publish it: the server
refused with `number-taken` - "WordPress already has a post numbered #217
(Hypocrisy)". The build itself was fine; the catalogue's numbering was one
below the site's.

## Root cause

The catalogue was imported from `UP-HYPOC.NFO`'s history block
(`scripts/import-history.ts`, see DEPLOYMENT.md). That NFO never listed
**#216 "All our Releases from GERP 2026"** (`UP-GRP26.LHA`, WordPress post
13849, 2026-02-08) - the release exists on the site but was never written
into the group's NFO history. The importer numbers sequentially by position,
so the missing entry pushed hypocrisy to 216 in the database while the site
had already published it as **#217** (post 13856, 2026-05-23, archive live at
`www.uprough.net/releases/UP-HYPOC.LHA`).

`nextReleaseNumber` is `MAX(release_number) + 1`, so the Evoke build drew 217
- a number the site had already used. The `number-taken` guard added in
`b245dee` caught it exactly as designed. Everything at 215 and below matches
WordPress; the drift begins at GERP 2026 and would have persisted forever.

## What was done (production, verified)

DB backup first: `/home/spot/releases.db.bak-2026-08-25-renumber`.

One transaction against `data/releases.db` on `hippoplayer`:

1. Deleted the `EVK26` draft that held 217 (guarded on `is_draft = 1`).
2. `UPHYPOC` 216 -> 217, display `'217'`, `published_at` and
   `wordpress_post_url` / `wordpress_post_id` 13856 filled in from the post.
3. Inserted `UPGRP26` at 216 - `UP-GRP26.LHA`, LHA, compomusic, creators
   "goto80, zabutom and h0ffman", post 13849, same imported-entry release
   notes as its siblings.
4. Bumped `meta.revision` (233 -> 234) so clients refetch.

Live after: `/api/releases/seed` reports `nextReleaseNumber 218` with 217
entries, `/release/UPGRP26` renders, `EVK26` is gone.

## Learnings

- The website's `#N:` post titles are the real public numbering. The NFO
  history block is a copy that can silently lose an entry, and the importer
  trusted it as complete. Any future import should be diffed against
  `wp-json/wp/v2/posts` titles before it becomes the source of truth.
- The already-shipped `UP-HYPOC.LHA` NFO says "release 216 UP-HYPOC.LHA".
  That cannot be fixed retroactively; the site and the catalogue now both
  say 217.
- `release_number` is UNIQUE, so an insert-in-the-middle has to free the
  number first (delete the draft, then bump, then insert) - the order matters.

## Next Steps

1. Varin0x rebuilds the Evoke 2026 release - it now draws **218** and its NFO
   history lists 216 GERP 2026 and 217 hypocrisy correctly. The `UP-EVK26.LHA`
   he already has is numbered 217 and must not reach traders.
2. Still outstanding from the previous handoff: push `uprough-releases-server`
   and `makerelease` - both still have their commits only locally.
