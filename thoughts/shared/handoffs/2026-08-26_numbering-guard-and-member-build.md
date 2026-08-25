---
date: 2026-08-26
topic: release-numbering root fix, catalogue repair, and the three-platform member build
tags: [makerelease, releases-doorserver, wordpress, catalog, numbering, packaging, distribution]
status: implemented
---

# Numbering guard, catalogue repair, and a fresh member build

Started from Varin0x being unable to publish ("Still reports it as Release 217
... Hypocrisy is already ID 217"), ended with the whole catalogue reconciled
against the website, the drift made impossible to miss, and new builds for all
three platforms sitting on the Desktop.

The detailed story of the numbering work is in
`2026-08-25_catalog-numbering-reconciliation.md` - read that one first if you
are touching numbering. This document is the session close: what state
everything is in and what is still open.

## Task(s)

1. **Varin0x cannot publish the Evoke 2026 build.** Done - the cause was a
   catalogue that was one behind the website. His build's number was wrong and
   the archive he holds (`UP-EVK26.LHA`, numbered 217) must not reach traders.
   He rebuilds and gets **218**.
2. **Stop it happening again** ("fix the root issue"). Done - see below.
3. **Fix the underlying history errors** ("patch the history or whatever" /
   "there may be human errors in the nfo"). Done - 25 filename mismatches
   found, 24 resolved, none outstanding.
4. **Ship a new build to members.** Done - macOS, Linux and Windows zips.

## State right now

- `https://releases.uprough.net/api/releases/numbering-check` reports
  **`ok: true`**, 0 ahead, 0 conflicts, 7 advisory `missingArchives`.
- Catalogue: 217 entries, **next release number 218**, no drafts.
- Server deployed and running (`pm2` id 0, `releases-doorserver`).
- Three distribution zips in `~/Desktop/up-rough-release-maker-0.1.0/`.
- Both repos are **pushed**: `spotUP/releases-doorserver` at `3438a3f`,
  `spotUP/makerelease` at its latest `main`. `makerelease`'s working tree is clean.

## Recent changes

### uprough-releases-server (4 commits on `main`)

| Commit | What |
|---|---|
| `b857bf9` | build-time number guard, `GET /api/releases/numbering-check`, `src/numbering.ts`, `insertHistoricalRelease`, `scripts/reconcile-numbering.ts` |
| `8f6de82` | compare against every archive a post links, not just the first |
| `3b485b5` | `WORDPRESS_RELEASES_DIR` is the arbiter for "does this archive exist" |
| `3438a3f` | `GET /seed` reports unpublished builds |

Tests 200 -> 246.

### makerelease (11 commits on `main`)

`a4b15c1` (editor numbering warning), `9378e30` (stale-draft warning,
`app/src/main/stale-drafts.ts`), plus the handoff docs. Tests 74 -> 101.

`35e14aa` then landed the DIZ release-line work that had been sitting
uncommitted: the banner art is inherited verbatim from the last published
release and only the single line naming the release can be retyped
(`app/src/renderer/diz-release-line.ts` replaces `diz-grid.ts`, which exposed
the whole 45x10 grid). `main.ts` carries the warning-display line in the same
commit, since it lives in the same `init()`. `b9edd19` ignores `UP-*` build
output in the repo root - about 450 MB of leftover `UP-HYPOC` artifacts are
still sitting there untouched, delete them if they are not wanted.

### Live data repairs (three passes, each with a DB backup on `hippoplayer`)

1. `releases.db.bak-2026-08-25-renumber` - inserted the missing GERP 2026 as
   216, moved hypocrisy 216 -> 217, deleted the abandoned EVK26 draft.
2. `releases.db.bak-2026-08-25-filenames` - 18 filename corrections plus the
   #108 duplicate.
3. `releases.db.bak-2026-08-25-round2` - 6 more, decided against the actual
   contents of `/var/www/uprough.net/releases`.

## Learnings

- **The NFO history block is the error-prone record, not the website.** The
  user said it plainly ("nfo files have errors as they are often written when
  i am tired") and the evidence agreed: 18 of 22 real mismatches were the
  history's side. New releases no longer type it - the history section is
  generated and the filename comes from `filenamesFor(code, format)`.
- **Pick the arbiter that cannot be stale.** Post links are editorial - the
  HivelyTracker 1.0 post says "Version 1.0 is outdated, instead;" and links
  1.2 on purpose. The releases directory is fact. Half the false positives
  came from asking the wrong record.
- **Verify before "fixing".** #115 was changed to the site's
  `ht16_m68k.lha` without probing it; that file does not exist and
  `ht15_m68k.lha` does. Reverted. The rule that worked everywhere else -
  the file that actually resolves wins - was simply not applied there.
- **electron-builder needs no Docker and no wine for Windows on macOS.** It
  ships a macOS-native `makensis`; `npx electron-builder --win` takes ~90
  seconds. The whole Docker detour was wasted: wine cannot run under Docker
  on Apple Silicon at all (`anon_mmap_fixed` assertion - 16 KB host pages),
  and the 157 KB "Setup .exe" it produced was a crash remnant. The clue was
  in the 2026-08-25 handoff all along: `npx electron-builder -mwl`.
- **Docker Desktop dies when the disk fills.** The disk hit 100% (399 MB free
  of 460 GB) mid-build; afterwards the backend processes were alive but the
  Linux VM was gone, and `docker ps` hung forever. Fix: force-kill
  `com.docker.backend` (a clean Quit is not enough), then relaunch.
- A `409 number-taken` at publish time costs a full rebuild, because the
  number is inside the NFO inside the archive. That is why the same check now
  runs at build time, before the number is handed out.

## Artifacts

- `~/Desktop/up-rough-release-maker-0.1.0/` - `Up_Rough_Release_Maker_-_macOS.zip`
  (194 MB), `_-_Linux.zip` (174 MB), `_-_Windows.zip` (156 MB). Each carries
  the same `INSTALL.txt` as the previous distribution, structure identical.
- `thoughts/shared/handoffs/2026-08-25_catalog-numbering-reconciliation.md` -
  the full numbering story, every repaired row, and the four cases that needed
  a human decision.
- `npm run reconcile-numbering` in `uprough-releases-server` - report mode
  prints drift; `--insert <n> ... --apply` backfills a missing release.

## Next Steps

1. **Send the zips to Varin0x and the crew.** He rebuilds the Evoke release
   and gets 218; the `UP-EVK26.LHA` he already holds says 217 and must be
   binned.
2. ~~Push both repos~~ **done**.
3. **Fix #194's download link** in wp-admin: the post links
   `up-xmas2018.zip`, the file is `UP-XMAS2018.ZIP`. Dead link from a case
   mismatch. (The user said they would do this.)
4. **Decide about the seven archives that are not on the website** - 34, 67,
   89, 104, 105, 202 (Santaz Triptro, missing under any name), 205. Upload or
   leave; `numbering-check` reports them as advisory.
5. **Windows still cannot pack archives** (carried over unchanged from
   2026-08-25): no `lha`/`zip` on the platform, so `INSTALL.txt` asks members
   to put `lha.exe`/`zip.exe` on PATH. The real fix is bundling them via
   electron-builder `extraResources`.
6. Optional: a `windows-latest` GitHub Actions job would keep builds off this
   laptop's disk entirely (`origin` is `git@github.com:spotUP/makerelease.git`).

## Other Notes

- Code signing / notarization still not done. macOS Gatekeeper needs the
  right-click-Open dance, Windows SmartScreen needs "Run anyway". Both
  documented in the INSTALL files.
- Disk went from 399 MB free to 12 GB during this session by clearing the
  Docker build tree, `app/release`, `~/Library/Caches/electron` and
  `~/Library/Caches/electron-builder`, and the 4.7 GB wine image. The two
  `releases-doorserver` Docker images (290 MB) were left alone. The Electron
  caches re-download (~850 MB) on the next build.
