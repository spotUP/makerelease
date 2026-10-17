---
date: 2026-08-24
topic: releases-doorserver Phase 2 - archive copy + WordPress publish pipeline
tags: [releases-doorserver, makerelease, wordpress, design]
status: draft
---

# Phase 2: archive copy + WordPress publish pipeline

## Context

Phase 1 (`2026-08-24-releases-doorserver-phase1-design.md`) makes
`releases-doorserver` the source of truth for release metadata and member
roster, and makes release-number assignment server-authoritative. It does
not touch how the actual release archive gets to `www.uprough.net` or how
its announcement post gets made - both are still fully manual today.

This phase automates that: after a release is built locally (Phase 1's
`buildRelease`, unchanged), a separate, explicit "Publish" action uploads the
archive to `www.uprough.net`'s `/releases/` path (confirmed live pattern:
`https://www.uprough.net/releases/UP-<CODE>.<FORMAT>`) and creates a
WordPress announcement post with a cover image.

This is deliberately a **separate phase and a separate spec**, not folded
into Phase 1, because it introduces a materially different trust boundary:
Phase 1's write endpoint only ever touches release metadata (text fields);
this phase's endpoint receives a large binary upload and, server-side, uses
real production server and WordPress credentials for `www.uprough.net`. It is
reviewable, buildable, and deployable independently of Phase 1's own
correctness.

## Why publish is a separate, explicit step (not automatic on build)

`UP-FB006`'s own release notes are direct evidence this matters: *"This is
the FINAL VERSION! A buggy party version was leaked, please replace it with
this bugfixed and enhanced version."* - a rough build has already, in this
group's real history, ended up public before it should have. Auto-publishing
the instant a local build finishes would remove the one chance to catch that
before it's live on the public site. Building stays local-only and
consequence-free, exactly as Phase 1 leaves it; publishing is a distinct,
deliberate action taken only when the member is satisfied with the build.

## Credential model

**`releases-doorserver` and `www.uprough.net` (WordPress) run on the same
box** (confirmed by the user - same host as `bbs.uprough.net`). This
simplifies the copy step to a local filesystem copy, not a network
hop - no remote-login key is needed at all. Only a WordPress Application
Password (WP's built-in mechanism, confirmed available) is required as a
real credential, held **only** on the `releases-doorserver` host as server
config (env vars, following the same "configured or refuse to start" policy
as every other credential in that project's `config.ts`). No member's
machine, and no copy of the Electron app, ever holds it. The app
authenticates to `releases-doorserver`'s publish endpoint the same way it
authenticates Phase 1's write endpoint (the shared `X-Release-Key` header)
and hands over the archive bytes + post content; the server does the local
file copy and the WordPress API call using its own credentials.

This also means the "publish" endpoint's file-copy step is simpler and more
reliable than a network copy would be (no connection/auth failure mode for
that half of the pipeline - a local `fs.copyFile` either succeeds or hits a
real filesystem error like disk-full or permissions, both worth surfacing
distinctly but neither is a network-flakiness concern).

## API

### `POST /api/releases/:code/publish` (authenticated, multipart)

Header: `X-Release-Key: <shared secret>` (same key as Phase 1's write
endpoint).

Multipart fields:
- `archive` - the built `.LHA`/`.ZIP` file bytes.
- `image` - the cover image file bytes.
- `code` must already exist as a `releases` row from Phase 1's `POST
  /releases` (the release's metadata - title, notes, category, creators,
  release_number - is read from that row, not re-submitted here, so the two
  steps can never disagree about what release they're describing).

Server-side steps, in order, each with its own error surfaced distinctly
back to the app (a partial failure - e.g. the copy succeeds, WordPress post fails
- must be reported as exactly that, not a generic "publish failed", so a
retry doesn't re-upload an archive that's already in place):

1. **Copy the archive** to `www.uprough.net`'s `/releases/` directory (a
   local filesystem path on this same host - see Credential model above) as
   `UP-<CODE>.<FORMAT>` (matching the already-confirmed live path exactly, so
   the download link Phase 1 already computes - `releases.uprough.net`'s
   `/manifest` and any future browsing page - is correct the moment this
   step completes). The exact source-of-truth local path for that directory
   (wherever the live site's `/releases/` URL
   actually resolves to on disk) is a deployment-time config
   value (`WORDPRESS_RELEASES_DIR` env var), not hardcoded - the operator
   sets it when deploying, matching this project's existing "configured, no
   defaults" convention.
2. **Upload the image** via the WordPress REST API's media endpoint
   (`/wp-json/wp/v2/media`), authenticated with a WordPress Application
   Password (confirmed available on this site), get back the WordPress media
   ID.
3. **Create the WordPress post** via `/wp-json/wp/v2/posts`:
   - Title: `#<release_number>: <release_name>` (the group's existing
     numbering convention, e.g. `#217: Hypocrisy` - confirmed by the user
     directly, not inferred).
   - Body: the release's `release_notes` text.
   - Featured image: the media ID from step 2.
   - **Category** (WordPress category, not a tag - corrected from an earlier
     draft of this spec): derived from the release's `entry_category` (the
     same fixed list already in the app's category `<select>` - Demo, Intro,
     Musicdisk, Music, Graphics, ASCII, Tool, Video, Science, Other), mapped
     1:1 to the group's real WordPress categories (confirmed full list:
     Demos, Music, Ascii, Diskmag, Graphics, Intros, Links for You,
     Musicdisks, News, Other, People, Radio Dirty, Science, Tools, Videos -
     the app's 10 categories all have a direct match):

     | App category | WordPress category |
     |---|---|
     | Demo | Demos |
     | Intro | Intros |
     | Musicdisk | Musicdisks |
     | Music | Music |
     | Graphics | Graphics |
     | ASCII | Ascii |
     | Tool | Tools |
     | Video | Videos |
     | Science | Science |
     | Other | Other |

     The WordPress-side categories the app never produces (Diskmag, Links
     for You, News, People, Radio Dirty) are simply never targeted - no
     mapping needed for them.
4. Record the publish result (WordPress post URL, archive copy completion) back onto
   the `releases` row (new columns: `published_at`, `wordpress_post_url`) so
   a second publish attempt on an already-published release can be detected
   and confirmed with the user rather than silently double-posting.

Response: `{ "archiveUrl": "https://www.uprough.net/releases/UP-HYPOC.LHA",
"wordpressPostUrl": "https://www.uprough.net/hypocrisy/" }`.

### Idempotency / re-publish safety

If `:code` already has `published_at` set, the endpoint returns `409` with
the existing `wordpressPostUrl` rather than re-running the pipeline - the app
surfaces this as "already published, here's the link" rather than silently
creating a duplicate WordPress post. A genuine re-publish (e.g. fixing a
typo'd post) is a deliberate follow-up action, out of scope for this spec's
v1 (WordPress's own edit UI already covers that case).

## makerelease app changes

- New "Publish" UI, separate from the existing "Build release" button -
  appears (enabled) only after a build has succeeded for the current form
  session, operating on the release `code` that was just built.
- New image-picker field, used only by this Publish step (not part of the
  build form) - a native file dialog restricted to image types, matching the
  existing `pickFilesAndDirs` IPC pattern in `ipc-handlers.ts`.
- Publish submits the archive (read back from the local path
  `finalizeRelease` already returned) + the picked image to `POST
  /api/releases/:code/publish`, shows progress (this is a real upload of a
  potentially 100+ MB file, unlike Phase 1's small JSON POST - the existing
  IPC request/response pattern in `preload.ts`/`ipc-handlers.ts` needs a
  progress-reporting channel here, noted as new work Phase 1 didn't need).
- On success, shows the returned archive URL and WordPress post URL. On a
  `409` (already published), shows the existing post URL instead of
  attempting again.

## Decisions confirmed (previously open questions)

- **Category mapping** (not tags - corrected above): full 1:1 table
  confirmed against the real WordPress category list.
- **File transfer**: `releases-doorserver` and `www.uprough.net` share a
  host, so this is a local file copy (`WORDPRESS_RELEASES_DIR` config path),
  not a network hop - no remote-login key needed.
- **WordPress auth**: Application Passwords, confirmed available on the real
  site.

## Testing

- `releases-doorserver`: unit tests for the publish pipeline's three steps
  mocked independently (local file copy, media upload, post creation) so a
  failure in any one step is tested in isolation, plus the idempotency
  (`409`) path. No test should perform a real WordPress API call - both
  external calls (media upload, post creation) are mocked in the test suite,
  matching door-repo's own pattern of never hitting real external systems in
  `npm test`. The file-copy step can be tested against a real temp directory
  (no mocking needed - it's local disk I/O, not a network call).
- Manual end-to-end (necessarily against real `www.uprough.net`, so run
  deliberately, not in CI): publish one real throwaway release, confirm the
  archive is downloadable at the predictable URL, confirm the WordPress post
  exists with the correct title format, image, tag, and body, then remove
  both afterward the same way Phase 1's manual archive-build verification
  already cleans up test artifacts.
