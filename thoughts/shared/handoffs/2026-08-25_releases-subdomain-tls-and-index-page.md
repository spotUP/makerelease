---
date: 2026-08-25
topic: releases.uprough.net TLS failure, subdomain vhost, browsable release index
tags: [releases-doorserver, makerelease, nginx, letsencrypt, deployment, dns]
status: implemented
---

# releases.uprough.net: TLS fix, own vhost, release index page

## Task(s)

Reported symptom: "releases.uprough.net has ssl issues no page loads".
All of the below is done, deployed and verified live. Nothing is mid-flight.

1. Diagnosed and fixed the TLS failure (wrong DNS target + missing vhost/cert).
2. Gave the subdomain its own nginx vhost and Let's Encrypt certificate.
3. Built a browsable release index page at `https://releases.uprough.net/`.
4. Switched the app's `DEFAULT_BASE_URL` to the subdomain.
5. Fixed an unrelated write-stream race found while running the test suite.

## Root cause of the reported bug

`releases.uprough.net` resolved to `89.167.21.154` — the **Hetzner box** that
runs `bbs`/`doors.uprough.net` under Caddy — not `97.75.89.139`
(`asciiarena.se`), where releases-doorserver actually runs. Caddy there had
no site block for the name, so no certificate was ever issued for it
(confirmed: crt.sh had zero entries), and every request died in the TLS
handshake with `alert internal error (80)` / `no peer certificate available`
before any HTTP was exchanged. Hence "SSL issue" **and** "no page loads" —
nothing HTTP-level was ever reached.

Diagnostic that settled it: on port 80 the Hetzner Caddy answered
`Host: releases.uprough.net` with a **308** — byte-identical to its answer
for a bogus hostname — while a genuinely configured host (`bbs.uprough.net`)
got a **301**. That distinguishes "configured but broken" from
"not configured at all". It was the latter.

The user then repointed the Loopia A record to `97.75.89.139`.

## Critical References

- Production host: ssh alias `hippoplayer` (`97.75.89.139`, `asciiarena.se`).
- New vhost: `/etc/nginx/sites-available/releases.uprough.net` (server only,
  not in git — same as every other vhost on this host).
- New renewal hook: `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`.
- `releases-doorserver/DEPLOYMENT.md` — updated; authoritative. Read it
  before touching the server.
- DB backup taken before deploying: `/home/spot/releases.db.bak-2026-08-25`.

## Recent Changes

### Server (hippoplayer, outside git)

- `releases.uprough.net` vhost. `:80` serves
  `/.well-known/acme-challenge/` from `/var/www/letsencrypt` behind a
  `location ^~` (which outranks the `location /` redirect) and redirects
  everything else. `:443` proxies `/` and `/api/releases` to
  `127.0.0.1:3020` and serves `/releases/` straight off `/data/releases/`.
- Certificate via `certbot certonly --webroot`, expires 2026-11-23.
- `renewal-hooks/deploy/reload-nginx.sh`, because `authenticator = webroot`
  with no installer renews the certificate but never reloads nginx — without
  it the cert would have silently gone stale ~90 days out. Verified by
  running the hook and by `certbot renew --dry-run`.
- The `www.uprough.net` `/api/releases` block is untouched and still serves.

### releases-doorserver

- `src/index-page.ts` (new): `buildArchiveIndex`, `resolveDownloads`,
  `renderIndexPage`. Dark monospace page, self-contained CSS, HTML-escaped.
- `src/catalog.ts`: `readReleaseIndex()` — own query rather than widening
  `FullHistoryRow`, which would change `/manifest`'s shape for every client.
- `src/app.ts`: `GET /` renders the index.
- `src/upload.ts`: **real bug fix** — `fail()` called
  `archiveWriteStream.destroy()` and immediately `rm`'d the partial archive,
  but `fs.createWriteStream` opens its fd asynchronously, so an open still
  in flight could complete *after* the delete and re-create the file. The
  resurrected file then broke the caller's recursive temp-dir cleanup with
  `ENOTEMPTY` (`routes.ts` removes its publish temp dir exactly that way).
  Now waits for the stream's `'close'` before unlinking.
- Tests: 136 -> 158. `tests/index-page.test.ts` (new) plus a temp-dir-empty
  assertion on the oversize-upload test.

### makerelease app

- `app/src/main/config.ts`: `DEFAULT_BASE_URL` is now
  `https://releases.uprough.net`; the old comment claiming the subdomain has
  no DNS record was removed.

## Learnings

- **Only 189 of 216 catalogue entries had a matching archive on disk.** The
  `filenames` column and the real filenames disagree in case
  (`UP-HYPOC.LHA` vs `up-scago.lha` vs `uprough-saukkopack-32.adf`), so
  matching is case-insensitive against a directory listing built per request.
- **Stem matching was measured and deliberately rejected.** Ignoring the
  extension would have recovered 8 more entries and got at least two wrong:
  `up-ms2k1.avi` resolves to a screenshot `Up-MS2k1.jpg`, and `up-amluv.lha`
  to `UP-AMLUV.TXT`. A link handing someone the wrong file is worse than no
  link. Exact-name matching only.
- **`filenames` is a list for four entries** (the HivelyTracker per-platform
  builds, space- and comma-separated). The whole trimmed string is tried as
  one filename first so names containing spaces survive; only then is it
  split. This took links from 189 to 198.
- **`published_at` is NULL for all 216 rows** — the whole catalogue came in
  through `scripts/import-history.ts`. Filtering the index on "published"
  would have rendered an empty page against production.
- The documented redeploy `rsync` had no `--exclude data`, and the live
  SQLite DB lives at `data/releases.db` *inside* the app directory. A working
  copy with a local `data/` would have overwritten the production catalogue.
  Fixed in the command and called out in `DEPLOYMENT.md`.
- The `www.uprough.net` vhost proxies `/.well-known/acme-challenge` to
  `127.0.0.1:4789`, where **nothing is listening**. Harmless today: that cert
  uses `authenticator = nginx`, which injects its own temporary block. Left
  alone as out of scope, but it is dead config waiting to confuse someone.

## Verification performed

- `https://releases.uprough.net/` — 200, 51 KB, 216 rows, 198 download links.
- Sample downloads 200: `UP-HYPOC.LHA` (189 MB), `UP-ACS25.ZIP`,
  `uprough-saukkopack-32.adf`, `ht16_aros.lha`.
- `/api/releases/health` returns JSON on both hostnames.
- `http://` redirects 301 to `https://`.
- `certbot renew --cert-name releases.uprough.net --dry-run` succeeded.
- releases-doorserver: typecheck clean, 158/158 tests, 8 consecutive full
  runs green (the upload flake previously hit roughly every second run).
- makerelease app: typecheck clean, 67/67 tests.

## Follow-up: the index page now clones the door repository's design

The first version of the page was designed from scratch and looked nothing
like `doors.uprough.net`. The reference is
`/Users/spot/Code/amiexpress-doorserver/web` — a React/Vite/Tailwind/Radix
SPA. What was cloned is its **design**, not its stack:

- Palette, type scale and table treatment come from that repo's
  `tailwind.config.js`, `components/DoorTable.tsx` and `pages/Browse.tsx` —
  same dark tokens (`--bg: #0b0d10`, `--accent: #5ac8fa`, ...), Inter with a
  mono column for identifiers, bordered rounded table card, uppercase surface
  header row, lucide's download glyph as the row action.
- Tokens are inlined as plain CSS in `src/index-page.ts`. Adding React, Vite
  and a build step to this Express service to emit one static table would
  cost far more than it returns. **If the door repo's tokens change, change
  them here too** — the comment on `STYLES` says so.
- The search field was brought over as well, since it is the most
  recognisable band of the reference layout. The catalogue is already in the
  document, so filtering is inline script with no request and no dependency;
  with JavaScript off every release still lists, just unfiltered. Verified
  live: 216 rows -> 22 for "goto80", count updates, no-match message shows.

Screenshotting both pages at 1440x900 and comparing caught three defects the
markup alone did not show: Category was absorbing several hundred pixels of
slack that belonged to Credits, the dash for a release with no archive was
not aligned with the download icons above it, and the page 404'd for a
favicon. All three are fixed.

## Next Steps

1. **Both repos have uncommitted changes** (see `git status` in each). The
   server is running code that is not yet committed anywhere.
2. Distributed member builds still point at `www.uprough.net`. Rebuild and
   redistribute only when there is another reason to; the old origin works.
3. Optional: the index page has no cover art, per-release detail pages, or
   filtering. It is a flat table on purpose.
