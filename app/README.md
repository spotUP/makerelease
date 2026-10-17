# makerelease

Electron app for building Up Rough release archives. Talks to the
`releases-doorserver` API for release numbering, the member roster, and
release history — see `src/main/releases-client.ts`.

## Distributing a real build

Publishing a release (`POST /api/releases` in `src/main/releases-client.ts`)
requires the `X-Release-Key` header, sourced from `RELEASES_WRITE_KEY`. A
packaged `.app` launched from Finder inherits no shell environment, so this
key must be baked in at **build time**, not read from `process.env` at
runtime.

To produce a real distributable build with a working write key:

```sh
RELEASES_WRITE_KEY=<the real key> npm run dist
```

This runs `scripts/inject-release-key.js` before compiling, which writes the
key into `src/main/config.generated.ts`. That file is gitignored and
regenerated automatically (`npm install`'s `postinstall` hook, and again by
`npm run dist`) — it always exists locally with either the real key or a
`null` placeholder, so plain dev builds and a fresh checkout keep compiling
without ever needing the key committed. `src/main/config.ts` imports that
constant and prefers it over everything else at runtime.

If you build without setting `RELEASES_WRITE_KEY`, the app falls back to the
`REPLACE_ME_BEFORE_DISTRIBUTING` placeholder key, which the server always
rejects with 401. The app detects this and shows a warning in the UI on
startup (`configWarning` in `SeedState`) instead of only logging to
main-process stdout, which a Finder-launched user would never see.

For local development against a local/staging `releases-doorserver`, set
`RELEASES_WRITE_KEY` (and optionally `RELEASES_API_BASE_URL`) as a normal
env var before `npm start` — no injection step needed; see the resolution
order documented at the top of `src/main/config.ts`.

## Uploading to Aminet

The Publish step has an "Also upload to Aminet" box, ticked by default. When it
is ticked, a successful publish to uprough.net is followed by an anonymous FTP
upload of the archive and a matching `.readme` to `ftp://main.aminet.net/new`,
as Aminet's uploading instructions ask. The readme is composed from the
description, release notes and credits of the form, previewed live before the
Publish button does anything, and written next to the archive (for example
`UP-HYPOC.LHA` gets `up-hypoc.readme`) so a failed upload can be redone by
hand. Uploader name, email, Aminet directory and architecture are remembered
in the app between releases.

The upload runs through `curl`, which ships with macOS, Windows 10 and later
and every Linux distribution. A missing `curl`, a refused upload or an
incomplete readme is reported next to the publish result; the website publish
itself is unaffected. Aminet is only tried after the website took the release,
never at build time, and never when the publish failed.

Aminet cannot be undone from the app: a Revoke removes the release from
uprough.net but the Aminet copy stays. Ask the Aminet team to remove it, or
upload a corrected archive under the same file name, which replaces it.

## Production server

`RELEASES_API_BASE_URL` defaults to `https://www.uprough.net` (see
`DEFAULT_BASE_URL` in `src/main/config.ts`) — `releases-doorserver` runs
there, path-mounted at `/api/releases/*` on the same host and nginx vhost as
the WordPress site (not on its own `releases.uprough.net` subdomain — that
DNS record doesn't exist). Full server-side deployment details — host,
paths, pm2 setup, nginx config, how to update it, how to rotate the
`RELEASES_WRITE_KEY`/WordPress credentials — live in
[`releases-doorserver/DEPLOYMENT.md`](https://github.com/spotUP/releases-doorserver/blob/main/DEPLOYMENT.md)
(separate repo). The real `RELEASES_WRITE_KEY` for that server is not
committed anywhere — get it from whoever ran the deploy, or generate a new
one and update the server's `ecosystem.config.cjs` to match.
