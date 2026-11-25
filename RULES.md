# Release Dropper rules

Project rules for Release Dropper (repo makerelease), the Tauri app that packs demoscene releases
from per-group profiles, and numbers and publishes Up Rough and /X Innovations
releases through their server. Global rules in
`~/.claude/CLAUDE.md` apply unless overridden here. `CLAUDE.md` is a pointer
to this file.

## Commands

Run from the repo root unless the row says otherwise. The app is the Tauri
port in `tauri/` (Rust backend `tauri/src-tauri/`); `app/` now holds only the
renderer and shared TypeScript it loads.

| Task | Command |
|---|---|
| Install | `(cd app && npm install) && (cd tauri && npm install)` |
| Start dev | `sh tauri/scripts/sync-web.sh && cd tauri && npx tauri dev` |
| Build renderer | `sh tauri/scripts/sync-web.sh` (runs `npm run build:renderer` in `app/`, copies into `tauri/web/`) |
| Build app | `sh tauri/scripts/sync-web.sh && cd tauri && npx tauri build` |
| Type-check | `cd app && npm run typecheck` (renderer and shared) |
| Lint | none configured; type-check is the gate |
| Format | none configured |
| Test (interactive) | `cd app && npx vitest` |
| Test (CI) | `cd app && npm test` (`vitest run`, suites under `test/unit/**/*.spec.ts`); `nice -n 19 cargo test --manifest-path tauri/src-tauri/Cargo.toml` |
| One test file | `cd app && npx vitest run test/unit/<name>.spec.ts` |
| Stop / cleanup | quit the app window (or Ctrl-C the `tauri dev` terminal) |

Validation before push: `cd app && npm run typecheck && npm test`, plus `cargo test` (row above) when `tauri/src-tauri/` changed.

### URLs

- Releases API, LIVE: `https://releases.uprough.net` (`server.baseUrl` in the profile; override with `RELEASES_API_BASE_URL`)
- Door repository, LIVE: `https://doors.uprough.net`
- Website, LIVE: `https://www.uprough.net`
- Aminet upload, LIVE and public once a moderator approves: `ftp://main.aminet.net/new`. No sandbox. Never exercise it outside a real release.
- Local: no local server in this repo. The server is the separate `releases-doorserver` repo; its `DEPLOYMENT.md` holds host, paths and pm2 setup.

## Layout

- `tauri/src-tauri/` Rust backend: build, publish, door repository client, Aminet upload. `tauri/web-shim/shim.js` maps the renderer's `window.api` onto Tauri commands.
- `app/src/renderer/` the form and previews. `app/src/shared/` code both sides use, including `divisions.ts`, the only place a division differs.
- `app/test/unit/` vitest suites; `app/test/fixtures/` golden NFO and DIZ files (the Rust tests read them too).
- Release output: packaged builds write to `~/Documents/<label> Releases/`; dev builds to `releases-out/<profile id>/` in the repo root.
- Group profiles: `<userData>/profiles/<id>/` (`profile.json`, `file_id.diz`, optional `release.nfo`).
- `thoughts/shared/` committed research, plans, handoffs. `thoughts/spot/` and `thoughts/searchable/` git-ignored.

## Project-specific rules

- Secrets: the write key lives in `profiles/<id>/profile.json` in the user data folder, never in the repo. Never commit a real `RELEASES_WRITE_KEY`. The private bundle's source folder `app/private-profiles/` holds the same key and is git-ignored for the same reason (the Tauri app seeds from `PRIVATE_PROFILES_DIR` or a bundled `private-profiles` resource).
- Generated NFO and DIZ files are byte-exact against fixtures. A change to the generator needs the golden test updated from a measured source, not retyped.
- Icons: none; the UI is text and ASCII tokens.
