---
date: 2026-09-20
topic: making the release maker build Up Rough /X Innovations releases too
tags: [makerelease, releases-doorserver, nfo, diz, x-innovations]
status: draft
---

# A division switch: Up Rough and Up Rough /X Innovations

Research: `thoughts/shared/research/2026-09-20_x-innovations-release-format.md`.
Reference release: `~/Downloads/up-md15` (`oo7`, More Doors Cursor Menu 1.5).

## Decisions taken (2026-09-20, with spot)

1. **/X numbering and history live on the server**, as a `division` column
   with its own `MAX()+1` sequence. Drafts, the number guard, revoke and the
   stored DIZ art all come along for free - and the /X release list needs
   exactly that stored DIZ art.
2. **/X publishes like Up Rough**: WordPress post plus archive upload.
3. **The /X roster and BOARDS table are edited in the app and stored
   server-side**, like the Up Rough roster.
4. **The /X NFO frame is a fixed template**; the tool fills in fields, notes,
   roster, boards and the release list.
5. **Post titles**: Up Rough stays `#218: <title>`, /X is `/X oo7: <title>`.
   The drift checker only reads titles starting with a bare `#<number>:`, so
   it keeps working untouched and never sees /X as a main-sequence number.

## The geometry, measured from UP-MD15.NFO

- Every content line is 74 columns: `   | %% |` (9) + a 59-wide content
  field + `| %% |` (6).
- Section rules are 77 columns, `- -| %% |` + 59 dashes + `| %% |- -`.
- A release-list entry is `  ooN:` on its own line, then that release's
  45-column DIZ art centred in the 59-wide field (7 spaces of indent).
- Display numbers use the group's old typography: `0` is written `o`, so
  release 7 is `oo7`. One formatter, shared with the legacy labels the
  catalogue already holds (`oo1`, `o1o`, `o99`).

**Open detail for implementation:** the `oo7` block embeds only two text
rows (PRESENTS + name) while the shipped `FILE_ID.DIZ` has three (PRESENTS +
name + tagline). Settle against the fixture - either the release list drops
the tagline row, or that entry was hand-trimmed. Ask dipswitch if the
fixture is ambiguous.

## Phase 1 - server: the division column

`uprough-releases-server`

- `src/db.ts`: boot-time migration adds `division TEXT NOT NULL DEFAULT
  'uprough'` to `releases` (same pattern as `is_draft`/`wordpress_post_id`,
  so the existing 218 rows are unaffected).
- `src/catalog.ts`: every read and write takes a division.
  `readNextReleaseNumber(db, division)` becomes
  `SELECT MAX(release_number) ... WHERE division = ?`, and `insertRelease`
  writes the row's division and formats `release_number_display` through the
  new `formatReleaseNumber(number, division)`.
- `getSeed`, `readHistory`, `getManifest`, `readReleaseIndex`,
  `readNumberedCatalog`, `readDrafts`: all filtered by division.
- `src/routes.ts`: `GET /api/releases/seed?division=...` (defaults to
  `uprough` so the shipped app keeps working), same for `/manifest`;
  `POST /api/releases` takes `division` in the body and validates it against
  the known set.
- `src/numbering.ts`: `compareNumbering` only ever receives `uprough` rows -
  the /X sequence has no `#N:` posts to compare against. Assert that in a
  test rather than leaving it implied.

Verification: existing 246 tests still pass unchanged (they are all
`uprough`), plus new tests for a /X row getting `oo1` while Up Rough is at
218, and for the two sequences not seeing each other.

## Phase 2 - server: /X roster and boards

- `members` gains a `division` column; `insertRelease`'s wholesale roster
  replacement becomes per-division.
- New `boards` table: `name`, `sysop`, `hq`, `status`, `sort_order`,
  `division`. Same replace-on-publish lifecycle as members, so the last /X
  release's boards seed the next one.
- Seed returns `members` and `boards` for the requested division.

Verification: a /X publish must not touch the Up Rough roster, and the
reverse. That is the test that would have caught the roster clobber this
design risks.

## Phase 3 - app: the switch and the DIZ

- `app/src/shared/divisions.ts` (new): one record per division holding its
  name, code prefix, banner art, which DIZ rows are editable, and the post
  title format. Everything division-specific reads from here - no `if
  (division === ...)` sprinkled through the renderer.
- Radio at the top of the form: **Up Rough** / **Up Rough /X Innovations**.
  Switching it reloads the seed for that division, so the release number,
  history and roster all follow.
- `app/src/shared/constants.ts`: `DIZ_RELEASE_LINE_ROW` and
  `DEFAULT_DIZ_BANNER` move into the division record. /X declares rows 9 and
  10 (name, tagline) as editable.
- `app/src/renderer/diz-release-line.ts`: already finds a row's writable span
  by its frame characters; extend it to edit a list of rows rather than one.
  Its 13 tests stay valid.

Verification: switching division changes the number preview, the banner and
the number of editable DIZ lines; a dry run for each division writes to
`dry-run/` and neither sees the other's history.

## Phase 4 - app: the /X NFO renderer

- `app/test/fixtures/UP-MD15.NFO`: the real release NFO, committed as the
  golden file.
- `app/src/main/nfo-x-innovations.ts` (new) renders the framed template:
  gutters, the field line, notes wrapped into the 59-wide field, the three
  roster groups, the boards table, and the release list of embedded DIZ art.
- **Golden test**: feed the renderer the inputs UP-MD15 was made from and
  assert the output is byte-identical to the fixture. That is the only
  honest proof the template is right, and it is the reachability test for
  this phase.
- The form grows /X-only fields, hidden for Up Rough: release date, coder,
  system, design, DIZ tagline, roster groups, boards table.

## Phase 5 - publish

- `src/post-content.ts` / the publish route: title comes from the division
  (`#${n}: ${title}` or `/X ${display}: ${title}`).
- The publish-time number guard (`findPostByReleaseNumber`) stays
  Up-Rough-only; for /X it would search a numbering the site does not use.
  Guard the guard with a test.

## Phase 6 - submit the door to the door repository

Decided 2026-09-20: a /X release is an AmiExpress door, so it belongs in the
door repo at `doors.uprough.net` as well. All four existing /X doors are
already listed there (`up-md15.lha`, `up-stw10.lha`, `up-dgc10.zip`,
`UP-MDCM1.ZIP`), submitted by hand.

- After a /X build, POST the archive to
  `https://doors.uprough.net/api/door-repo/submissions` as multipart, the
  way any member would. **No admin key in the app**: the member builds are
  unsigned zips anyone can unpack, so a curation key baked into one is a key
  everybody has. Approval stays a click in the door-repo admin UI.
- The server does the validating (`amiexpress-doorserver/src/submissions.ts`):
  extension allowlist `.lha/.lzx/.lzh/.dms/.zip`, magic bytes checked against
  the extension, 8 MB cap, and a sha256 check against both the catalog and
  the queue. A rebuild of the same archive comes back 409 with a readable
  reason rather than queueing twice - surface that message as-is.
- Show the 202's `message` and the submission `id` in the build result, so
  there is something to quote when approving.
- **Up Rough releases do not do this.** Demos, music and ASCII are not doors;
  only the /X division submits.

Verification: a /X build submits exactly once and reports the queue message;
an Up Rough build never calls the door server; a 409 duplicate is shown as
the server worded it, not swallowed.

## Risks

- **The roster is replaced wholesale on every publish.** With two divisions
  that becomes two rosters in one table; getting the division filter wrong
  wipes the wrong one. Phase 2's test exists for exactly this.
- **The drift checker must keep ignoring /X posts.** If somebody later
  retitles a /X post to `#7:`, the guard will read it as Up Rough 7. Worth a
  note in the checker's header comment.
- **Byte-identical NFO output is a hard target.** Trailing spaces inside the
  gutters, the tagline question above, and hand-tuned alignment will each
  cost a round. Budget for it; the fixture makes each one visible instead of
  shipping wrong art to traders.

## Not in scope

Migrating the seven existing /X releases into the catalogue. They can be
backfilled with `insertHistoricalRelease` once the division column exists -
their DIZ art is in the NFO's release list, which is the data the backfill
needs.
