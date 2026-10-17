# makerelease rules

Project rules for makerelease, the Electron app that numbers, packs and
publishes Up Rough and /X Innovations releases. Global rules in
`~/.claude/CLAUDE.md` apply unless overridden here. `CLAUDE.md` is a pointer
to this file.

## Commands

All commands run from `app/`.

| Task | Command |
|---|---|
| Install | `npm install` (postinstall writes `src/main/config.generated.ts` with a null key) |
| Start dev | `npm start` (builds, then launches Electron) |
| Build | `npm run build` (main, preload, renderer into `dist/`) |
| Type-check | `npm run typecheck` |
| Lint | none configured; type-check is the gate |
| Format | none configured |
| Test (interactive) | `npx vitest` |
| Test (CI) | `npm test` (`vitest run`, suites under `test/unit/**/*.spec.ts`) |
| One test file | `npx vitest run test/unit/<name>.spec.ts` |
| Distributable build | `RELEASES_WRITE_KEY=<real key> npm run dist` (DMG, x64 and arm64) |
| Stop / cleanup | quit Electron; stale DMG: `hdiutil info`, then `hdiutil detach <device>` |

Validation before push: `npm run typecheck && npm test`.

### URLs

- Releases API, LIVE: `https://releases.uprough.net` (`DEFAULT_BASE_URL` in `src/main/config.ts`; override with `RELEASES_API_BASE_URL`)
- Door repository, LIVE: `https://doors.uprough.net`
- Website, LIVE: `https://www.uprough.net`
- Aminet upload, LIVE and public once a moderator approves: `ftp://main.aminet.net/new`. No sandbox. Never exercise it outside a real release.
- Local: no local server in this repo. The server is the separate `releases-doorserver` repo; its `DEPLOYMENT.md` holds host, paths and pm2 setup.

## Layout

- `app/src/main/` Electron main: build, publish, door repository client, Aminet upload.
- `app/src/renderer/` the form and previews. `app/src/shared/` code both sides use, including `divisions.ts`, the only place a division differs.
- `app/test/unit/` vitest suites; `app/test/fixtures/` golden NFO and DIZ files.
- Release output goes to `~/Documents/Up Rough Releases/`. A leftover `UP-<CODE>/` in the repo root blocks a build of that code.
- `thoughts/shared/` committed research, plans, handoffs. `thoughts/spot/` and `thoughts/searchable/` git-ignored.

## Project-specific rules

- Secrets: `src/main/config.generated.ts` is generated and git-ignored. Never commit a real `RELEASES_WRITE_KEY`.
- Generated NFO and DIZ files are byte-exact against fixtures. A change to the generator needs the golden test updated from a measured source, not retyped.
- Icons: none; the UI is text and ASCII tokens.
