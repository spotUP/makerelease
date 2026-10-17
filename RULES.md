# makerelease rules

Project rules for makerelease, the Electron app that packs demoscene releases
from per-group profiles, and numbers and publishes Up Rough and /X Innovations
releases through their server. Global rules in
`~/.claude/CLAUDE.md` apply unless overridden here. `CLAUDE.md` is a pointer
to this file.

## Commands

All commands run from `app/`.

| Task | Command |
|---|---|
| Install | `npm install` |
| Start dev | `npm start` (builds, then launches Electron) |
| Build | `npm run build` (main, preload, renderer into `dist/`) |
| Type-check | `npm run typecheck` |
| Lint | none configured; type-check is the gate |
| Format | none configured |
| Test (interactive) | `npx vitest` |
| Test (CI) | `npm test` (`vitest run`, suites under `test/unit/**/*.spec.ts`) |
| One test file | `npx vitest run test/unit/<name>.spec.ts` |
| Distributable build | `npm run dist` (DMG, x64 and arm64; the build carries no key) |
| Windows build | `npm run dist:win` (from macOS: portable `.exe` only, x64, unsigned; electron-builder fetches its own Wine) |
| Stop / cleanup | quit Electron; stale DMG: `hdiutil info`, then `hdiutil detach <device>` |

Validation before push: `npm run typecheck && npm test`.

### URLs

- Releases API, LIVE: `https://releases.uprough.net` (`server.baseUrl` in the profile; override with `RELEASES_API_BASE_URL`)
- Door repository, LIVE: `https://doors.uprough.net`
- Website, LIVE: `https://www.uprough.net`
- Aminet upload, LIVE and public once a moderator approves: `ftp://main.aminet.net/new`. No sandbox. Never exercise it outside a real release.
- Local: no local server in this repo. The server is the separate `releases-doorserver` repo; its `DEPLOYMENT.md` holds host, paths and pm2 setup.

## Layout

- `app/src/main/` Electron main: build, publish, door repository client, Aminet upload.
- `app/src/renderer/` the form and previews. `app/src/shared/` code both sides use, including `divisions.ts`, the only place a division differs.
- `app/test/unit/` vitest suites; `app/test/fixtures/` golden NFO and DIZ files.
- Release output: packaged builds write to `~/Documents/<label> Releases/`; dev builds to `releases-out/<profile id>/` in the repo root.
- Group profiles: `<userData>/profiles/<id>/` (`profile.json`, `file_id.diz`, optional `release.nfo`).
- `thoughts/shared/` committed research, plans, handoffs. `thoughts/spot/` and `thoughts/searchable/` git-ignored.

## Project-specific rules

- Secrets: the write key lives in `profiles/<id>/profile.json` in the user data folder, never in the repo. Never commit a real `RELEASES_WRITE_KEY`.
- Generated NFO and DIZ files are byte-exact against fixtures. A change to the generator needs the golden test updated from a measured source, not retyped.
- Icons: none; the UI is text and ASCII tokens.
