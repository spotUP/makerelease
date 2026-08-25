---
date: 2026-08-25
topic: releases.uprough.net TLS failure, subdomain vhost, browsable release site
tags: [releases-doorserver, makerelease, nginx, letsencrypt, deployment, dns, frontend]
status: implemented
---

# releases.uprough.net: TLS fix, own vhost, and a browsable release site

Reported symptom: *"releases.uprough.net has ssl issues no page loads"*.

Everything below is done, deployed and verified live. Nothing is mid-flight.
The only open items are in **Next Steps** — neither repo has been pushed.

## Task(s)

1. Diagnosed and fixed the TLS failure. **Done.**
2. Gave the subdomain its own nginx vhost and Let's Encrypt certificate.
   **Done.**
3. Built a browsable release site at `https://releases.uprough.net/`, cloning
   the design of the door repository (`doors.uprough.net`). **Done.**
4. Sortable columns and a release detail view, to finish that parity. **Done.**
5. Switched the app's `DEFAULT_BASE_URL` to the subdomain. **Done.**
6. Fixed an unrelated write-stream race found while running the test suite.
   **Done.**

## Root cause of the reported bug

`releases.uprough.net` resolved to `89.167.21.154` — the **Hetzner box** that
runs `bbs`/`doors.uprough.net` under Caddy — not `97.75.89.139`
(`asciiarena.se`), where releases-doorserver actually runs. Caddy there had no
site block for the name, so no certificate was ever issued for it (crt.sh had
zero entries), and every request died in the TLS handshake with
`alert internal error (80)` / `no peer certificate available` before any HTTP
was exchanged. Hence "SSL issue" **and** "no page loads" — nothing HTTP-level
was ever reached.

The diagnostic that settled it: on port 80 the Hetzner Caddy answered
`Host: releases.uprough.net` with a **308**, byte-identical to its answer for a
bogus hostname, while a genuinely configured host (`bbs.uprough.net`) got a
**301**. That distinguishes "configured but broken" from "not configured at
all". It was the latter. The user then repointed the Loopia A record to
`97.75.89.139`.

## Critical References

- **Production host**: ssh alias `hippoplayer` (`97.75.89.139`,
  `asciiarena.se`), user `spot`, passwordless sudo. Same host as
  `www.uprough.net`'s WordPress.
- **Deploy doc**: `releases-doorserver/DEPLOYMENT.md` — updated this session,
  authoritative. **Read it before touching the server.**
- **Design reference**: `/Users/spot/Code/amiexpress-doorserver/web`
  (`tailwind.config.js`, `components/DoorTable.tsx`,
  `components/DoorDetail.tsx`, `components/DizView.tsx`, `pages/Browse.tsx`),
  live at `doors.uprough.net`.
- **New vhost**: `/etc/nginx/sites-available/releases.uprough.net` (server
  only, not in git — same as every other vhost on this host).
- **New renewal hook**: `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`.
- **DB backup** taken before the first deploy:
  `/home/spot/releases.db.bak-2026-08-25`.

## Recent Changes

### Server (hippoplayer, outside git)

- **`releases.uprough.net` vhost.** `:80` serves
  `/.well-known/acme-challenge/` from `/var/www/letsencrypt` behind a
  `location ^~` (which outranks the `location /` redirect) and redirects
  everything else. `:443` proxies `/` and `/api/releases` to `127.0.0.1:3020`
  and serves `/releases/` straight off `/data/releases/`.
- **Certificate** via `certbot certonly --webroot`, expires **2026-11-23**.
- **`renewal-hooks/deploy/reload-nginx.sh`**, because `authenticator = webroot`
  with no installer renews the certificate but never reloads nginx — without
  it the cert would have silently gone stale ~90 days out. Verified by running
  the hook and by `certbot renew --dry-run`.
- The `www.uprough.net` `/api/releases` block is **untouched and still serves**.

### releases-doorserver (6 commits, all on `main`)

| Commit | What |
|---|---|
| `a50f417` | fix: wait for the archive stream to close before deleting its file |
| `0029400` | feat: serve a browsable release index at `GET /` |
| `cf53a4d` | docs: document the releases.uprough.net vhost and certificate |
| `ceadecf` | feat: match the door repository's design on the release index |
| `fc0dc0a` | feat: sortable columns on the release listing |
| `7948fa4` | feat: add a release detail view |

New modules: `src/page-chrome.ts` (shared palette, icons, document shell),
`src/index-page.ts` (listing), `src/release-detail.ts` (detail).
`src/catalog.ts` gained `readReleaseIndex()` and `readReleaseDetail()`.
`src/app.ts` gained `GET /`, `GET /release/:code` and a `/fonts` static mount.
Topaz vendored into `public/fonts/` (two TTFs from the door repo).

Tests: **136 → 181**, 12 suites.

### makerelease (4 commits, all on `main`)

`8061eeb` points `app/src/main/config.ts`'s `DEFAULT_BASE_URL` at
`https://releases.uprough.net`; the rest are this handoff. App tests 67/67.

## Learnings

**Archive matching.** Only **189 of 216** catalogue entries had a matching
archive on disk. The `filenames` column and the real filenames disagree in
case (`UP-HYPOC.LHA`, `up-scago.lha`, `uprough-saukkopack-32.adf` coexist), so
matching is case-insensitive against a directory listing built per request.

**Stem matching was measured and deliberately rejected.** Ignoring the
extension recovers 8 more entries and gets at least two wrong: `up-ms2k1.avi`
resolves to a screenshot `Up-MS2k1.jpg`, `up-amluv.lha` to `UP-AMLUV.TXT`. A
link handing someone the wrong file is worse than no link. Exact-name only.

**`filenames` is a list for four entries** (HivelyTracker per-platform builds,
space- and comma-separated). The whole trimmed string is tried as one filename
first so names containing spaces survive; only then is it split. 189 → 198
links.

**The catalogue is almost entirely empty of prose.** Of 216 rows:
`published_at` is NULL for all, and **0 have DIZ art, 0 have news, 0 have a
description**. All 216 share one placeholder `release_notes` ("imported
historical entry, no original release notes preserved"). The history import
had none of it. Two consequences:
- Filtering the index on "published" would render an empty page.
- The FILE_ID.DIZ tab is **conditional**, the way the door repo shows its Guide
  tab only for a door with documentation. An unconditional tab would be an
  empty tab on every release on the site. **Release pages are therefore thin
  today — that is the data, not the layout.** Releases made through the app
  carry description, credits, notes and DIZ and get the full treatment
  automatically.

**Design tokens now live in two places.** `src/page-chrome.ts` inlines the door
repo's palette and type scale as plain CSS rather than pulling in React, Vite,
Tailwind and Radix to emit two server-rendered pages. **If the door repo's
tokens move, move them here too** — there is a comment on `STYLES` saying so.

**Sorting belongs in the page here.** All 216 rows are already in the document,
so re-ordering costs a `DocumentFragment` and no request, and it composes with
the search filter that a server-side sort would have had to re-implement.
Number sorts on the internal integer: the display label is legacy free text
(`o94`, `034`, `oo8`) and sorting on it puts `o15` after `99`.

**Screenshot comparison caught what markup review did not.** Diffing the two
pages at 1440x900 surfaced three defects invisible in the HTML: Category was
absorbing several hundred pixels of slack belonging to Credits, the dash for a
release with no archive was not aligned with the download icons above it, and
the page 404'd for a favicon.

**A latent deploy footgun.** The documented redeploy `rsync` had no
`--exclude data`, and the live SQLite DB is `data/releases.db` *inside* the app
directory. A working copy with a local `data/` would have overwritten the
216-release catalogue. Fixed in the command and called out in `DEPLOYMENT.md`.

**Dead config, left alone.** The `www.uprough.net` vhost proxies
`/.well-known/acme-challenge` to `127.0.0.1:4789`, where **nothing is
listening**. Harmless today — that cert uses `authenticator = nginx`, which
injects its own temporary block — but it is waiting to confuse someone.

**The upload race was real, not a flaky test.** `receivePublishUpload`'s
`fail()` destroyed the archive write stream and immediately unlinked the
partial file, but `fs.createWriteStream` opens its fd asynchronously, so an
open still in flight could complete *after* the unlink and re-create the file.
The resurrected file broke the caller's recursive temp-dir cleanup with
`ENOTEMPTY` — and `routes.ts` removes its publish temp dir exactly that way, so
this could have surfaced in production on any upload rejected by the size cap.

## Verification performed

- `https://releases.uprough.net/` — 200, 216 rows, 198 download links.
- Sample downloads 200: `UP-HYPOC.LHA` (189 MB), `UP-ACS25.ZIP`,
  `uprough-saukkopack-32.adf`, `ht16_aros.lha`.
- Search: 216 → 22 rows for "goto80", count updates, no-match message shows,
  clears back to 216.
- Sorting: number 1..216 and back, title alphabetical, arrows and `aria-sort`
  toggle and clear on the other columns.
- Detail: modal opens and closes, `/release/UPHYPOC` 200,
  `/release/UPHYPOC?panel=1` returns a shell-less fragment, `/release/NOPE`
  404s, Topaz serves, **zero console errors**.
- `/api/releases/health` returns JSON on both hostnames; `http://` 301s.
- `certbot renew --cert-name releases.uprough.net --dry-run` succeeded.
- releases-doorserver: typecheck clean, 181/181 tests; 8 consecutive full runs
  green after the upload fix (it previously failed roughly every second run).
- makerelease app: typecheck clean, 67/67 tests.

## Artifacts

- This handoff.
- `releases-doorserver/DEPLOYMENT.md` — rewritten sections on the vhost, the
  certificate and the redeploy command.

## Next Steps

1. **Neither repo is pushed.** releases-doorserver is **6 commits** ahead of
   `origin/main`, makerelease **4**. The server is running code that exists
   only on this machine.
2. `UP-HYPOC.LHA` and `UP-HYPOC/` sit untracked in the makerelease root — build
   artifacts, not in `.gitignore`, so they surface in every `git status`.
   Decide: ignore them or delete them.
3. Distributed member builds still point at `www.uprough.net`. That origin
   works and its nginx block must not be removed. Rebuild and redistribute only
   when there is another reason to.
4. Not built, and not asked for: cover art, per-release detail beyond the
   stored fields, category filtering, pagination.

## Other Notes

- The two Up Rough sites are now meant to read as one. Any future change to the
  listing or detail design should start by looking at what
  `amiexpress-doorserver/web` does.
- `releases.uprough.net` and `www.uprough.net/api/releases/*` both proxy to the
  same pm2 process on `127.0.0.1:3020`. Both must keep working.
