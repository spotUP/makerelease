---
date: 2026-10-17
plan: thoughts/shared/plans/2026-10-17-generalise-release-maker.md
branch: feature/group-profiles
status: passed with open items
---

# Validation: generalise makerelease for any scene group

## Automated checks
- `npm run typecheck`: exit 0.
- `npm test`: 32 files, 305 tests pass (after fix 279e6c3; 31/302 before it).
- `npm run build`: exit 0.
- Public repo `spotUP/makerelease` main: `app/` tree identical to branch at 0c71391. Fix 279e6c3 is not on it yet.

## Plan coverage
All implement checklist items P0.1-P5.5 are done with commits (ledger `thoughts/spot/todos.md`). Phase 4 README items are present (profiles folder, placeholder table, MIT licence). Server-path grep over app, RULES.md, README and shared plans/research: no hits.

## Issues found and fixed
1. Stale publish state on profile switch (blocking). Build under profile A, switch to profile B, press Publish: A's archive and code went to B's server. Fixed in 279e6c3: `app/src/renderer/built-release.ts` binds the build to its profile; Publish, Aminet upload and revoke use `builtReleaseFor`; `init()` calls `resetPublishState()`. Regression test `app/test/unit/built-release.spec.ts` fails with the profile check removed.
2. Stale compiled files in `dist/` (part of the key leak below). `npm run build` now removes `dist/` first (279e6c3).

## Issues open
1. Real write key can ship in the DMG (blocking for `npm run dist`). `app/src/main/config.generated.ts` (git-ignored, holds the real key) still exists on disk; `tsconfig.main.json` compiles all of `src/main/**/*.ts`, and `build.files` packages `dist/**/*`. The plan's fix is: move the key into `<userData>/profiles/uprough/profile.json`, then delete the file. The uprough profile folder does not exist yet in `~/Library/Application Support/makerelease/profiles/`.

## Minor deviations (not blocking, not fixed)
- Credit is required for generic groups too (the creator line check runs for every renderer).
- `init()` has no race token: two fast profile switches can apply seeds out of order.
- Placeholder names outside `[a-z_]+` (for example `{{Group}}`) are left as text, not reported as unknown.
- `loadProfile` reads the file before validating the id.
- A UTF-8 BOM in `profile.json` breaks parsing.
- Blank block lines keep trailing whitespace.

## Manual checks (only a person can tick these)
- Launch with an empty userData folder: example profile appears, form fits, Open profiles folder works.
- Edit a template, press Reload profiles: preview updates.
- Build a release with each renderer (uprough, xinnovations, placeholder); NFO matches the golden file.
- Doorserver profile: publish, revoke, Aminet upload (Aminet is untested live; first real run at the next release).
- Profile with backend none: publish section hidden.
- Build under one profile, switch profile: Publish and Aminet sections disappear.
- Packaged DMG launches and carries no key (`grep` the key in `app.asar`).
