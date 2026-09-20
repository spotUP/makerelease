---
date: 2026-09-20
topic: what an Up Rough /X Innovations release is, and how it differs from an Up Rough one
tags: [makerelease, nfo, diz, x-innovations, research]
status: final
---

# Up Rough /X Innovations releases

Source analysed: `~/Downloads/up-md15` - "mORE dOORS cURSOR mENU 1.5", the
latest /X release, dated 2026-06-20, coded by zALO, NFO by diPSWiTCH.

## It is a separate release track, not a skin

| | Up Rough | /X Innovations |
|---|---|---|
| Numbering | catalogue on releases.uprough.net, currently at 218 | its own sequence, `oo1`..`oo7`; UP-MD15 is `oo7` |
| Known to the server | yes, every release is a catalogue row | **no** - `MD15`/`UPMD15`/`UPXMD15` all return `exists:false` |
| On www.uprough.net | yes, a numbered post per release | **no** - no post for More Doors |
| NFO layout | plain 80 columns, section headings | framed ANSI, every line inside `| %% |` gutters |
| NFO history | text rows: `release 217 / title / category / creator(s)` | the previous releases' **DIZ art**, embedded and indented inside the frame, one block per release |
| Roster in NFO | `nickname   role` list of the whole group | `sYSOPS / cODERS / aRTiSTS` groups, plus a `bOARDS` table (board, sysop, HQ status) |
| DIZ text rows | 2: a fixed "Up Rough Presents:" and one release line | 3: a fixed "uP rOUGH /X iNNOVATiONS pRESENTS:", the release name, and a tagline |
| Distribution | archive uploaded to the site, WordPress post created | BBS only, as far as the site and catalogue show |

## The /X NFO, section by section

1. Logo art (fixed), `PRESENTS`, release name.
2. A field line inside the frame:
   `rELEASE dATE [2026-06-20]  cODER [zALO/uP!..]  sYSTEM [aMiEXPRESS]  dESIGN [diNO/uP!..]`
3. `rELEASE nOTES` - free text, every line inside the `| %% |` gutters.
   In this release it carries Description / Components / Install / Main
   Functions sub-headings, all hand-written prose.
4. `uP/X mEMBERS` - three role groups, names separated by ` . `.
5. `bOARDS` - fixed-width table of board, sysop, HQ tag, status, then the
   site URL.
6. `rELEASE lIST` - `oo7:` down to `oo1:`, each entry being that release's
   45-column DIZ banner indented inside the frame, separated by a
   `- -| %% |---...---| %% |- -` rule.
7. Footer credit line: `+ nFO bY diPSWiTCH/uP^dS!^iMP - pARTY liKE iT'S 1996! +`

## The DIZ

Same 45-column logo art as Up Rough's, which is why the release list can
embed it. The difference is the text rows:

```
Up Rough                          /X Innovations
8 |            Up Rough Presents:      9 |     uP rOUGH /X iNNOVATiONS pRESENTS:
9 |  HYPOCRISY - A500 HDD DEMO ...    10 |        mORE dOORS cURSOR mENU 1.5
                                      11 |      uPDATES 4 A bETTER eXPERIENCE!
```

So `DIZ_RELEASE_LINE_ROW` (a single constant today, `app/src/shared/constants.ts`)
becomes per-division, and /X needs two editable rows rather than one.

## What this means for the tool

The renderer, the NFO generator and the DIZ editing all assume one division:

- `app/src/main/nfo-generator.ts` renders the Up Rough layout and its text
  history rows.
- `app/src/shared/constants.ts` holds one `BANNER`, one `DEFAULT_DIZ_BANNER`
  and one `DIZ_RELEASE_LINE_ROW`.
- `app/src/renderer/diz-release-line.ts` edits exactly one line.
- `buildRelease` always fetches the seed, always takes a catalogue number,
  and publishing always means WordPress plus an archive upload.

None of that is wrong for Up Rough; all of it is division-specific.

## Open questions (need answers before a plan)

1. **Where does /X numbering and history live?** The catalogue already
   stores each release's DIZ text, which is exactly what the /X release list
   needs - but it has one numbering sequence and assumes every row is a
   website release.
2. **Does a /X release publish anywhere?** Nothing on the site suggests so.
3. **Where do the /X roster and the BOARDS table come from?** They are not
   the Up Rough member roster the server serves.
4. **Is the /X NFO art fixed?** The footer credits diPSWiTCH for the NFO, so
   presumably the frame is a fixed template and only fields, notes, roster,
   boards and the release list vary.
