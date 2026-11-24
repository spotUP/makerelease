# makerelease: developer notes

User documentation is in the [root README](../README.md). This page is for
working on the app itself.

## Commands

The app is the Tauri port in `../tauri/`; this folder holds only the
renderer (the window) and the shared TypeScript it uses. The Tauri build
compiles it with `npm run build:renderer` through `tauri/scripts/sync-web.sh`.

```sh
npm install
npm run typecheck    # renderer and shared
npm test             # vitest run, suites under test/unit/
npx vitest run test/unit/<name>.spec.ts
npm run build:renderer
```

Run the app from the repo root: `sh tauri/scripts/sync-web.sh && cd tauri && npx tauri dev`.

## Private groups

`app/private-profiles/` (git-ignored; the group folders, key included, never
the example) is what a private build seeds on first launch instead of the
example. The Tauri app reads it from `PRIVATE_PROFILES_DIR` or a bundled
`private-profiles` resource. Seeding copies missing ids only, so reinstalling
never overwrites a group the user edited.

## Layout

- `src/renderer/` the form and its previews.
- `src/shared/` code the renderer uses: profile parsing, the placeholder
  template language, naming, the DIZ release line, the Aminet readme.
- `test/unit/` vitest suites. `test/fixtures/` byte-exact NFO and DIZ files,
  also read by the Rust tests in `tauri/src-tauri/`.

## How a build flows

The backend is Rust in `tauri/src-tauri/src/`:

1. `profiles.rs` loads `profile.json` and the templates from the user data
   folder.
2. `net.rs` holds the backends: `none` builds on disk only,
   `releases-doorserver` numbers and publishes through a server.
3. `build.rs` renders the outputs (`placeholder` fills the group's
   templates, `uprough` and `xinnovations` draw the two built-in layouts),
   stages the release folder and runs `lha` or `zip`.

## Golden output

The `uprough` and `xinnovations` renderers must keep producing the same
bytes. The `ur_golden`, `ex_golden` and `x_golden` tests in
`tauri/src-tauri/src/build.rs` compare them with `test/fixtures/`. Update a
fixture only from a measured source, never by retyping it.

## Up Rough and /X Innovations profiles

These groups use `"backend": "releases-doorserver"` with a `server` entry:

```json
"server": { "baseUrl": "https://releases.uprough.net", "writeKey": "<key>" }
```

Builds carry no key; the key lives in the profile on the machine that
publishes. Two environment variables override the profile for local work
against another server:

| Variable | Overrides |
|---|---|
| `RELEASES_API_BASE_URL` | `server.baseUrl` |
| `RELEASES_WRITE_KEY` | `server.writeKey` |

Without a valid key the server rejects a publish with 401, and the window
says so on startup. The server is the separate `releases-doorserver`
repository, which documents its own deployment.

## Aminet upload

The upload runs `curl` against `ftp://main.aminet.net/new`. There is no
sandbox: an upload is public once a moderator approves it, so never exercise
it outside a real release. Tests replace `curl`.
