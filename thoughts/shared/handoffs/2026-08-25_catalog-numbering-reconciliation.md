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

## Stopping it happening again (code, committed, NOT yet deployed)

The data fix above does not prevent a repeat: anyone can post a numbered
release to the site by hand and the catalogue will drift again. Three changes,
`uprough-releases-server` commit `b857bf9` and `makerelease` commit `a4b15c1`:

1. **The site's numbering is now readable.** `listNumberedPosts`
   (`src/wordpress-client.ts`) pages through the posts and returns every
   `#<number>: <name>` title plus the archive that post links under
   `/releases/`. `src/numbering.ts` compares that against the catalogue and
   reports two things: releases the site has that the catalogue does not
   (`ahead`), and numbers that describe a *different* release on each side
   (`conflicts`, matched by archive filename - what would have caught GERP
   2026 the day it was posted).
2. **The number is checked at build time**, in `POST /api/releases`, before it
   is handed out and baked into the NFO. A number the site has already used is
   refused with 409 `number-taken` and nothing is written, so the code stays
   free for the rebuild. The identical publish-time check stays as the
   backstop. WordPress being unreachable does not block a build - the risk is
   a rebuild, and refusing to let anyone work because the website is down is
   worse.
3. **The drift is visible before anyone builds.** `GET
   /api/releases/numbering-check` exposes the comparison, and makerelease's
   `loadSeedState` asks for it alongside the seed and surfaces
   `numberingWarning` in the editor next to the release number.

Repair, when it does happen: `insertHistoricalRelease` (`src/catalog.ts`) plus
`npm run reconcile-numbering` - no arguments prints the drift, `--insert
<number> ... --apply` backfills the missing release and shifts the entries
above it up, which is exactly the surgery done by hand above.

Server tests 200 -> 236, app tests 74 -> 93 (the numbering guard was verified
by disabling it and watching its test fail).

## The 25 filename mismatches the check found on its first live run

Deploying the check immediately turned up 25 numbers where the catalogue and
the site disagreed about which archive the release is. Three were the check's
own fault and 18 were real errors in the imported NFO history.

**Check bug (fixed, `uprough-releases-server` commit `1cfd4f7`).** A release
post often links several archives - side A and side B of the same LP, every
build of HivelyTracker, a follow-up fix - and only the first link was being
compared. Six numbers (27, 50, 51, 90-93 partly, 131, 132) were flagged purely
because the catalogue's file was the post's *second* link.

**Real errors in the history (fixed in the live catalogue).** Every fix was
decided by an HTTP request to `www.uprough.net/releases/<name>`: the new name
resolves, the old one 404s. Backup first:
`/home/spot/releases.db.bak-2026-08-25-filenames`.

| # | was | is |
|---|---|---|
| 15 | up-intro.lha | up-yours.lha |
| 40 | up-tumbl.lha | up-tumb.lha |
| 47 | up-style.mp3 | up-style.lha |
| 94 | up-2teis.lha | up-2teis.zip |
| 131/132 | up-l3ang/up-l3bng.lha | up-lp3an/up-lp3bn.lha |
| 133 | up-gbahx.lha | up-gbahx.zip |
| 151 | up-d13gx.lha | up-ds13gx.lha |
| 159 | up-13pce.zip | up-14pce.zip |
| 166 | up-dunno.lha | up-dunno.zip |
| 181 | up-tbsne.zip (#180's file) | up-fuck3.lha |
| 182 | up-e2015.zip | up-e2015.lha |
| 194 | up-bbaza.lha (#193's file) | up-xmas2018.zip |
| 201 | up-xmas2020.zip (#200's file) | up-256.zip |
| 206 | up-blvd.lha | up-blvrd.lha |
| 209 | ..._flafouk.zip | ..._glafouk.zip |
| 213 | up-scago.lha | up-scago.zip |

**#34's columns were swapped.** The history line read `release 034 sige dame
maz` with `sige_dame_maz.mp3` in the title column - the filename and the title
had traded places. Now title "sige dame maz", filename `sige_dame_maz.mp3`.

**#115 was left alone.** The site's post links `ht16_m68k.lha`, which does not
exist on the web server; the history's `ht15_m68k.lha` does. Changing it to
match the post was a mistake and was reverted - the rule is "the file that
actually resolves wins", and here that is the history's.

**#108 was a duplicate.** The history had "anniversary" (up-annv.lha) at both
108 and 113, and the release the site has at #108 - "Up Garage Treat mix",
UP-GRTMX.LHA - was missing from it entirely. #108 is now that release.

25 conflicts -> 4.

## Four left, each needing a human decision

| # | site | catalogue | what is actually on the web server |
|---|---|---|---|
| 90 | Hively Tracker 1.0, links ht12.lha | ht10.lha | `HT10.LHA` exists - the POST is the stale one |
| 91 | Hively Tracker 1.1, links ht12.lha | ht11.lha | neither ht11.lha nor HT11.LHA exists - the 1.1 archive is gone |
| 93 | "Hively Tracker 1.3" (ht13.lha, ht13_m68k.lha) | "hively tracker 1.2 (68k version)" (ht12_m68k.lha) | both files exist - the two records disagree about what #93 IS |
| 202 | Santaz Triptro, links up-xmas2021.7z | up-xmas2021.zip | neither exists - the archive is not on the server at all |

90, 91 and 194's dead link are WordPress-side fixes (relink the post, or
re-upload the file). 93 is a numbering question only the group can answer.

## Auditing every filename, not just the disputed ones

A follow-up pass asked the web server about all 217 rows. Once mixed-case
names (`TBLvsUP.lha`, `UPvsAPT.LHA`, `SpbUP-BW.lha`) are accounted for, 12
rows name a file that is not online: 8, 67, 75, 89, 91, 104, 105, 171, 202,
204, 205 and 34's original value. Those are not history errors - they are
releases whose archive was never put on the website (BBS-era `.dms`, an
`.avi` party report, an HTML release, a `.mp3` that lives under `/radio/`).
Only #34 was a real mistake, and it is fixed above.

Worth saying plainly: the history block is the error-prone side, because it
was hand-typed. **New releases no longer type it at all** - the NFO's history
section is generated from the catalogue and the filename comes from
`filenamesFor(code, format)`, so the class of typo repaired here cannot be
introduced by the tool. What still gets typed by a human is the title, the
creators and the release notes.

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

1. ~~Deploy the server~~ **done** - deployed and verified live; for reference:
   `rsync -az --exclude node_modules --exclude .git --exclude dist --exclude coverage --exclude data ./ hippoplayer:/home/spot/apps/releases-doorserver/`
   then `ssh hippoplayer 'cd /home/spot/apps/releases-doorserver && npm ci && npm run build && pm2 restart releases-doorserver'`,
   then check `curl -s https://releases.uprough.net/api/releases/numbering-check`
   reports `ok: true` against the repaired catalogue.
2. Decide the four remaining conflicts above; until then the editor shows a
   numbering warning on every load.
3. Ship a new makerelease build so the editor shows the numbering warning
   (`npm run dist` in `app/`). Not urgent: the server-side build gate protects
   the existing build too.
4. Varin0x rebuilds the Evoke 2026 release - it now draws **218** and its NFO
   history lists 216 GERP 2026 and 217 hypocrisy correctly. The `UP-EVK26.LHA`
   he already has is numbered 217 and must not reach traders.
2. Still outstanding from the previous handoff: push `uprough-releases-server`
   and `makerelease` - both still have their commits only locally.
