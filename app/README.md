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
