---
date: 2026-08-24
topic: releases-doorserver Phase 1 - metadata API + makerelease app integration
tags: [releases-doorserver, makerelease, api, design]
status: draft
---

# Phase 1: releases-doorserver metadata API + makerelease app integration

## Context

The `makerelease` Electron app (this repo, `app/`) currently seeds its member
list, next release number, and NFO release history by scanning a local
`makerelease/` directory for the most-recent `UP-XXXXX/UP-XXXXX/UP-XXXXX.NFO`.
This means:

- Any Up Rough member who wants to run the tool needs a local copy of the
  most recent release folder, kept up to date by hand.
- Two members building a release concurrently from stale local copies could
  independently compute the same next release number - nothing prevents a
  collision.

The fix is a small hosted service, `releases-doorserver`, forked from the
existing `amiexpress-doorserver` project (`/Users/spot/Code/amiexpress-doorserver`,
which already serves an analogous public catalog - the AmiExpress door
repository - at `bbs.uprough.net`). Forking reuses a proven stack and design
(Express, better-sqlite3, TypeScript, Docker, public read API pattern)
instead of inventing one from scratch.

This phase makes the makerelease app depend on that service instead of local
files, and makes release-number assignment server-authoritative (fixing the
collision risk as a side effect). It does **not** include archive upload,
SCP-to-www.uprough.net, or WordPress publishing - that is Phase 2
(`2026-08-24-releases-doorserver-phase2-publish-design.md`), a separately
reviewable and deployable unit with a different trust boundary (it needs
production SSH/WordPress credentials; this phase needs none).

## Architecture

New repo, `releases-doorserver`, forked from `amiexpress-doorserver`. Same
stack and conventions:

- Express, pinned to an exact version (matching the door-server's own pinning
  rationale - see its README).
- `better-sqlite3` for storage.
- TypeScript, Jest for tests.
- Docker deploy, run alongside the existing `bbs.uprough.net` deployment on
  the same box. DNS (`releases.uprough.net` -> that box) and reverse-proxy
  routing are the user's responsibility; this phase delivers the app and a
  Dockerfile, not server access.

No file storage. The real archives already live at
`https://www.uprough.net/releases/<filename>` (confirmed live: both
`UP-HYPOC.LHA` and `UP-FB006.LHA` resolve there with `200` and correct
`Content-Length`). The download link for any release is always computed as
`https://www.uprough.net/releases/UP-<CODE>.<FORMAT>` - never stored, never
out of sync.

Public read endpoints are open (CORS `*`, no auth), matching door-repo's own
policy: this is a public, read-only catalog. The one write endpoint is
authenticated by a static shared-secret header (see Auth section) - no
login UI, no JWT/session machinery. The app "will be private" (not publicly
distributed), which is why a single baked-in secret is an acceptable trust
model for v1, unlike door-repo's JWT+scrypt admin-login system (which exists
because that server also has a public admin web UI reachable by anyone).

## Data model (SQLite)

```sql
CREATE TABLE IF NOT EXISTS releases (
  id               INTEGER PRIMARY KEY AUTOINCREMENT,
  code             TEXT NOT NULL UNIQUE,       -- 'FB006', 'HYPOC'
  release_number   INTEGER NOT NULL UNIQUE,    -- server-assigned, atomic
  format           TEXT NOT NULL,              -- 'LHA' | 'ZIP'
  release_name     TEXT NOT NULL,              -- quoted display name
  platform_tag     TEXT,                       -- 'A1200', optional
  release_notes    TEXT NOT NULL,
  news             TEXT NOT NULL,
  diz_text         TEXT NOT NULL,
  entry_title      TEXT NOT NULL,
  entry_category   TEXT NOT NULL,              -- one of the fixed category list
  entry_creators   TEXT NOT NULL,
  created_at       INTEGER DEFAULT (strftime('%s','now'))
);
CREATE INDEX IF NOT EXISTS idx_releases_number ON releases(release_number);

CREATE TABLE IF NOT EXISTS members (
  nickname   TEXT PRIMARY KEY,
  role       TEXT NOT NULL,
  updated_at INTEGER DEFAULT (strftime('%s','now'))
);
```

`members` is a whole-roster snapshot, not per-release history: every
`POST /releases` call includes the full current member table (as edited in
the app's memberlist UI) and the server replaces the table contents to match
- the same whole-list-replace semantics the app's `MemberlistTable` already
has locally. This matches how the roster has always worked (an "active
members" list that changes over time, not a per-release fact).

A `meta` table holds a single `revision` integer, incremented on every
successful write (release insert or member replace), for the
`X-Release-Repo-Revision` header - mirroring door-repo's own revision/update-
detection pattern (`docs/DOOR-REPO-API.md` section 7).

## API

All JSON endpoints return `Content-Type: application/json`. Public endpoints
carry `Access-Control-Allow-Origin: *`, matching door-repo's CORS policy
(see that project's `src/cors.ts` for the exact headers to replicate).

### `GET /api/releases/seed` (public)

Replaces the app's local `repo-scanner.ts`. Response:

```json
{
  "members": [{ "nickname": "spot", "role": "leader, ink, stereofonik development" }, ...],
  "nextReleaseNumber": 217,
  "history": [
    { "releaseNumber": 216, "filenames": "up-hypoc.lha", "title": "hypocrisy",
      "category": "demo", "creators": "flubba, goto80, bldzr, mortimer twang, spot" },
    ...
  ],
  "currentDizText": "    _ _ __ ___    ____  _ _ __ _________\n   ...",
  "revision": 42
}
```

`nextReleaseNumber` is a live preview only, not authoritative - the real
number is assigned atomically by `POST /releases` at build time (see below).
`history` carries every past release's four NFO fields, in descending
`release_number` order, so the app's `nfo-generator.ts` can still render the
full "released so far" block exactly as it does today from locally-parsed
data - only the data source changes, not the generation logic.

`currentDizText` is the most recent release's raw `diz_text` column (the
already-shipped diz overwrite-grid in the app seeds itself from this field on
load, same behavior as today's local-file version - `toGridText()` pads or
truncates it to the fixed 45x10 grid exactly as it already does). `null`
when the catalog is empty.

### `GET /api/releases/manifest` (public)

Full JSON list of releases + members, for a future browsing web UI (Phase 1
does not build that UI - see "Out of scope" below). Same shape as `/seed`
plus every release's remaining fields (`releaseName`, `platformTag`,
`releaseNotes`, `news`, `dizText`).

### `POST /api/releases` (authenticated)

Header: `X-Release-Key: <shared secret>`. `401` if missing or wrong.

Body: every release field except `release_number`, plus the full current
`members` array:

```json
{
  "code": "TEST0", "format": "LHA", "releaseName": "'MY RELEASE'",
  "platformTag": "A1200", "releaseNotes": "...", "news": "...",
  "dizText": "...", "entryTitle": "...", "entryCategory": "Demo",
  "entryCreators": "...", "members": [{ "nickname": "spot", "role": "..." }, ...]
}
```

Server, in one write transaction: `SELECT MAX(release_number)+1`, `INSERT`
the release row, `DELETE FROM members` + re-`INSERT` the submitted roster,
increment `revision`. This is the fix for the concurrent-build race described
in Context - two simultaneous `POST /releases` calls serialize through
SQLite's write lock and get distinct, correctly-incremented numbers.

Response:

```json
{
  "releaseNumber": 217,
  "revision": 43,
  "history": [
    { "releaseNumber": 217, "filenames": "up-test0.lha", "title": "...",
      "category": "Demo", "creators": "..." },
    { "releaseNumber": 216, "filenames": "up-hypoc.lha", "title": "hypocrisy",
      "category": "demo", "creators": "..." },
    ...
  ]
}
```

`history` is the full, freshly-read history array (same shape as `/seed`'s
`history` field) **including the just-inserted release** as its first entry,
so the app's `nfo-generator.ts` always renders the NFO's "released so far"
block from the authoritative post-insert state - not from whatever was
cached in the renderer since the app was opened. This matches the old
local-scan behavior exactly: `ipc-handlers.ts`'s `buildRelease` always
re-scanned the local NFO fresh immediately before generating, rather than
reusing `loadSeedState`'s earlier snapshot - the API-backed version preserves
that same freshness guarantee, just from the server's perspective instead of
the filesystem's.

`400` on a `code` that already exists, or a missing required field.

### `GET /api/releases/health` (public)

`{ "status": "ok", "revision": 43, "releases": 217 }`, mirroring door-repo's
`/health` shape.

## Auth

A single env var, `RELEASES_WRITE_KEY` (or a small `label:key` list, reusing
door-repo's `parseAdminKeys` parsing pattern from `src/config.ts`, purely for
audit-labeling which member published - no login flow either way). Startup
fails if unset, same "configured or refuse to start" policy as door-repo's
`config.ts`. No JWT, no sessions, no password hashing - this is a single
static secret compared with `crypto.timingSafeEqual`, not a login system.

## makerelease app changes

- `src/main/repo-scanner.ts` is replaced by a new `src/main/releases-client.ts`
  that calls `GET /api/releases/seed` over HTTP. `loadSeedState` in
  `ipc-handlers.ts` calls this instead of scanning the filesystem.
- `buildRelease` in `ipc-handlers.ts` changes order: **first** `POST
  /api/releases` (get the server-assigned `releaseNumber`), **then**
  generate the NFO/diz using that number (today's `nfo-generator.ts` logic,
  unchanged), stage, pack, and place the archive locally exactly as today.
  The physical archive is still built entirely on the member's own machine;
  this phase does not change where the file ends up (Phase 2 does).
- No local-file fallback. If the network call to `releases.uprough.net`
  fails, `loadSeedState`/`buildRelease` fail with a clear, specific error
  (already-existing error-surfacing pattern in `main.ts`'s `init()` and
  submit handler covers this - just a different failure source).
- The write key ships in the app's own config (e.g. a `src/shared/constants.ts`
  value baked in at build time, not a user-facing settings field) - consistent
  with "app will be private, no login UI."
- `codeExists` (currently a local filesystem check in `repo-scanner.ts`)
  becomes a server-side check: either folded into `POST /releases`'s `400`
  response, or a small `GET /api/releases/:code/exists` if the app still
  wants live-typing feedback before submit (current UX has a debounced
  check-as-you-type on the code field - worth keeping, so this phase adds
  that endpoint too, public, cheap).

## Out of scope for Phase 1

- Any public browsable web page (Phase 1's `/manifest` endpoint exists for a
  future one, but no frontend ships in this phase).
- Upload of the built archive to `www.uprough.net`.
- WordPress post publishing.

Both are Phase 2.

## Testing

- `releases-doorserver`: unit tests for the atomic release-number assignment
  under concurrent requests (the whole point of moving this server-side),
  auth middleware (missing/wrong/correct key), and the seed/manifest
  response shapes - following the existing project's Jest conventions
  (`tests/*.test.ts` per module).
- `makerelease` app: `releases-client.ts` tested against a mock HTTP server
  (or `nock`-style interception) the same way `nfo-parser`/`nfo-generator`
  are tested today against fixtures - no live network call in the test
  suite.
- Manual end-to-end: run `releases-doorserver` locally, point the app at it
  via a local config override, build a real throwaway release, confirm the
  returned `releaseNumber` is used correctly in the generated NFO and that a
  second concurrent build gets the next number, not a collision.
