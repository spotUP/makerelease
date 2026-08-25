---
date: 2026-08-25
topic: DIZ leading-space bug, draft releases, full revoke, and the 217/218/219 cleanup
tags: [makerelease, releases-doorserver, wordpress, catalog, deployment]
status: implemented
---

# Draft releases, full revoke, and the 217/218/219 cleanup

Two user reports from Varin0x, both fixed, both deployed. The production
catalogue has been cleaned up and now ends at release 216, with 217 free.

## Task(s)

1. **DIZ bug**: "If the first line of the .DIZ ascii text starts with spaces,
   those are deleted." **Done.**
2. **Phantom releases**: a build that is never published still consumed a
   release number and appeared in the public index, so one intended release
   read as 217, 218 and 219. **Done** - unpublished builds are now invisible
   drafts.
3. **Full revoke**: remove a failed release completely - catalog row, release
   number, published archive, WordPress post. **Done.**
4. **Live cleanup** of 217/218/219. **Done.**

## Root causes

**DIZ.** `renderDiz` called `form.dizBannerText.trim()`
(`app/src/main/nfo-generator.ts`). `trim()` strips leading whitespace from the
whole string, which is exactly the first line's indentation. Replaced with
`trimDizBanner()`: trailing spaces/tabs per line and blank leading/trailing
rows go (that is the DIZ grid's 45-column padding), leading spaces stay.

**Phantom releases.** `buildRelease` POSTs to `/api/releases` at *build* time,
not publish time (`app/src/main/ipc-handlers.ts`), because the release number
is baked into the NFO the build packs. `insertRelease` inserted a fully
visible row, so an abandoned build was permanent and public. The number cannot
move to publish time - the NFO needs it - so the fix is that the row starts as
a **draft**.

## What changed

### uprough-releases-server (commit `78a3ec1`, on `main`, not pushed)

- `is_draft` column (default 0, so the whole imported catalogue is unaffected)
  and `wordpress_post_id`, both added by the existing boot-time migration in
  `src/db.ts`.
- `insertRelease` creates drafts by default (`isDraft: false` only from
  `scripts/import-history.ts`). Drafts are excluded from `GET /`,
  `GET /release/:code`, `/seed`'s history, `/manifest` and `currentDizText`.
  They still reserve their release number and their code.
- One deliberate exception: `insertRelease` returns history *including* the
  draft it just created (`readFullHistory(db, includeDraftCode)`), because a
  release's own NFO has to list itself.
- `markPublished` clears `is_draft` and records the post id.
- `DELETE /api/releases/:code` now also trashes the WordPress post
  (`trashPost`, no `force`, so it is recoverable in wp-admin for 30 days).
  Rows published before `wordpress_post_id` existed fall back to
  `findPostIdByUrl`, a slug lookup on the stored post URL. A post that cannot
  be trashed does NOT fail the request - the catalog row is already gone by
  then and a retry would 404 - so the response carries
  `wordpressPostTrashed` and `wordpressPostError` instead.
- Latest-only deletion is unchanged: numbering stays contiguous.
- Tests 181 -> 200.

### makerelease (commits `a7d175a`, `24d7505`, on `main`, not pushed)

- `trimDizBanner` in `app/src/main/nfo-generator.ts`, exported and also used
  by `releases-client.ts`'s `toPublishBody`, so the catalog copy of the DIZ is
  trimmed identically to the `file_id.diz` (the release page on the web was
  otherwise rendering the grid's 45-column padding).
- `app/src/renderer/revoke-message.ts` - `describeRevoke()`, extracted and
  tested because a revoke touches four things that can each end in a different
  state.
- Cleanup section is now "Revoke this release"; the build message says the
  release is a draft until published and costs nothing to abandon.
- `DeleteReleaseResult` gained `wasDraft`, `wordpressPostTrashed`,
  `wordpressPostError`.
- Tests 69 -> 74.

## Production cleanup (done, verified)

DB backup first: `/home/spot/releases.db.bak-2026-08-25-revoke`.

Deployed with the documented rsync + `npm ci && npm run build && pm2 restart`
flow, then revoked newest-first (the only order the latest-only rule allows):

| Code | Number | Result |
|---|---|---|
| `EVV26` | 219 | row gone, `UP-EVV26.LHA` deleted, WordPress post trashed via the slug fallback |
| `EVO26` | 218 | row gone (was never published) |
| `EVK26` | 217 | row gone (was never published) |

Verified live: `/api/releases/seed` reports `nextReleaseNumber 217` with 216
entries; `releases.uprough.net/release/EVV26`,
`www.uprough.net/releases/UP-EVV26.LHA` and the WordPress post all 404.

## Learnings

- The three Evoke entries were one release attempted three times. Only 219 was
  ever published; 217 and 218 were catalog-only stubs. That distinction is
  only visible via `published_at` / `wordpress_post_url`, not from the index.
- Deleting the rows does **not** fix the archive that already shipped: the NFO
  inside `UP-EVV26.LHA` embeds a history block listing 217 and 218. That is
  why the agreed fix was a full redo rather than a renumber.
- `set -a && . ./.env` breaks on this server - `WORDPRESS_APP_PASSWORD`
  contains spaces. Extract single values with `grep`/`sed` instead.
- The live draft-invisibility path has NOT been exercised in production yet
  (an attempted throwaway POST was blocked by the local tool sandbox). It is
  covered by tests; the first real confirmation will be Varin0x's next build -
  the index must stay at 216 entries until they press Publish.

## Next Steps

1. Varin0x rebuilds the Evoke release and publishes it - it will land as 217
   with a correct NFO history.
2. Ship a new app build so they get the draft/revoke wording (`npm run dist`
   in `app/`); the existing build still works against the new server.
3. Push both repos - `uprough-releases-server` and `makerelease` still have
   these commits only locally.
