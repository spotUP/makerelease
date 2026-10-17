---
date: 2026-10-17
topic: generalising makerelease into a public release maker for any scene group
tags: [makerelease, generalise, templates, aminet, profiles, public-release]
status: final
---

# Generalising makerelease: research index

Two companion documents hold the measured facts:

- `2026-10-17_generalise-coupling.md`: every point where the app depends on
  Up Rough or on `releases-doorserver`, and the form-to-publish data flow.
- `2026-10-17_generalise-templates.md`: how the Up Rough and /X NFO and DIZ
  renderers work, variable inventory, formatting rules, golden-test state.

## Decisions already made (2026-10-17, by spot)

1. Generalise in place. This repo becomes the public project. Up Rough is the
   first group profile.
2. The generic product publishes to Aminet only. No website, no door
   repository, no numbering, no database. Release number and filenames are
   typed by hand when a template has a slot for them.
3. Up Rough and /X keep their code renderers. Generic groups load their own
   `FILE_ID.DIZ` and `.NFO` files with simple placeholders. No directive
   language.
4. License MIT.
5. `releases-doorserver` stays in the codebase only as the optional backend
   Up Rough's profile selects. Generic users never see it.

## What is (summary of the companion docs)

- The app has no local catalogue. Numbering, roster, boards, history, the
  previous DIZ art and the WordPress publish all come from the server. A
  non-dry build POSTs to the server first and refuses to run without it; a
  dry run still needs `GET /seed`.
- The server surface the app uses: `GET /seed`, `GET /:code/exists`,
  `POST /`, `DELETE /:code`, `POST /:code/publish`. Write key in the
  `X-Release-Key` header, baked in at build time by
  `app/scripts/inject-release-key.js`.
- Group identity is spread over about ten files: `UP-` prefix, 5-character
  code, output folder `Up Rough Releases`, NFO headings and URLs in
  `constants.ts`, closed `DivisionId` union, WordPress wording in
  `revoke-message.ts`, `www.uprough.net` and `division.label` in the Aminet
  readme.
- Door-repo submission sends no key. Revoke works only on the latest
  release. Build metadata (date, coder, system, design) never reaches the
  server. No test hits a live server.
- Output is Latin-1, LF. NFO writer throws above U+00FF; DIZ is
  transliterated to ASCII. Only /X has a golden test; Up Rough's NFO has
  `toContain` checks only.
- Placeholders cannot express the Up Rough or /X layouts (fixed-width
  frames, ceil-centring, three wrap styles, computed columns, grouped
  roster). Hence decision 3.

## Architecture insight for the plan

The seam already exists in `app/src/shared/divisions.ts`: a record that
describes what differs, consumed everywhere, branched nowhere. A group
profile is that record grown to cover everything the coupling doc lists,
plus two new axes:

- `renderer`: `uprough` | `xinnovations` | `placeholder` (template files).
- `backend`: `none` | `releases-doorserver` (base URL, key from the profile
  file, not from a build-time injection).

Everything the server used to seed (roster, boards, previous art, history)
is absent under `backend: none`, so generic placeholder templates list only
what the form holds. The Aminet upload is already backend-independent and
needs only the group name and URL from the profile.

## Open questions for the plan phase

1. Placeholder syntax and the exact set. Candidates from the form: group,
   release name, code, filenames, format, date, coder, system, design,
   credits, notes, release line (DIZ), members as a repeated line. Decide
   whether a repeated block exists at all or members are a joined string.
2. Where a profile lives and how it is chosen: one JSON file plus the two
   template files in the app data directory, picked from a Settings screen,
   or a profile file opened from the menu.
3. What the Up Rough profile looks like on disk, and how the write key moves
   from `inject-release-key.js` to it (or stays baked for Up Rough's own
   builds).
4. Output root naming per profile (`~/Documents/<group> Releases/`), dev
   output root.
5. App name and repo name for the public release.
6. Which form sections hide under `backend: none`: division radio, boards,
   member list, media embeds, creators autocomplete, revoke.
7. Phase 5 hygiene: new history (orphan or new repo), `thoughts/shared`
   content that names hosts and backups, LICENSE, README for groups.
