---
date: 2026-10-17
topic: How the current NFO and FILE_ID.DIZ generators work (input for a template engine)
tags: [nfo, diz, templates, x-innovations, golden-tests]
status: final
---

# Current NFO / DIZ generation, as built

All paths relative to `app/`. "Unconfirmed" marks anything not verified in code.

## 1. Up Rough NFO (`src/main/nfo-generator.ts`, `renderNfo` l.62-108)

Output = array of lines joined with `\n` (l.78-107). No wrapping of notes/news. No transliteration of the NFO (only DIZ is transliterated); the writer rejects chars > U+00FF.

Top to bottom (constants in `src/shared/constants.ts`):

| # | Region | Kind | Source |
|---|--------|------|--------|
| 1 | `BANNER` (constants.ts l.14-25): 10 ASCII-art lines, FIRST element is an empty line (NFO starts with `\n`), last art line is 18 spaces (trailing spaces kept), no trailing newline in the constant | fixed art | constant |
| 2 | blank | fixed | |
| 3 | `REL_NOTES_HEADING` = 8 spaces + `[ r e l e a s e   n o t e s ]` | fixed | l.7 |
| 4 | blank | | |
| 5 | release line: 8 spaces + `Release named {releaseName}` + (platformTag ? ` ({platformTag})` : '') + ` (UP-{CODE.toUpperCase()}.{format})` | generated | l.63-65, 83 |
| 6 | optional: 8 spaces + `Released {releaseDate.trim()}`; line omitted entirely if date empty/absent (no blank left) | generated, conditional | l.71-73, 84 |
| 7 | blank | | |
| 8 | release notes: `form.releaseNotesText.trim()` then every NON-empty line prefixed with 8 spaces (`indentBlock`, l.20-25); empty lines stay empty; no wrap, no width limit; `\r` not stripped (unconfirmed whether CRLF input is normalised upstream; not in this file) | generated | l.86 |
| 9 | blank, `NEWS_HEADING` (8 sp + `[ n e w s ]`), blank | fixed | l.8 |
| 10 | news: same `indentBlock(newsText.trim())` | generated | l.90 |
| 11 | blank, `MEMBERS_HEADING` (8 sp + `a c t i v e   u p   r o u g h   h e r o e s`), blank | fixed | l.9 |
| 12 | members: one line per member, 8 spaces + `nickname.padEnd(W)` + `role`, W = max(20, longest nickname length + 2) (l.27-32). Order as entered. No wrap. | generated, repeated rows, computed column | |
| 13 | 2 blanks, `ONLINE_LINE` (`        online         www.uprough.net`), 2 blanks | fixed | l.11 |
| 14 | `HISTORY_HEADING` (8 sp + `r e l e a s e d   s o   f a r`), blank | fixed | l.10 |
| 15 | history block: entries joined by ONE blank line, newest first (order = server's `history`). Each entry (l.34-52): see below | generated, repeated block | |
| 16 | 2 blanks, `FOOTER_LINE` (8 sp + ``c a t c h `e m   a l l   a t   www.uprough.net``), then `''` so file ends with a single `\n` | fixed | l.12, 103-106 |

History entry (`renderHistoryEntry`): constants INDENT=8 (`HISTORY_BODY_INDENT`), LABEL_FIELD=12, CONTINUATION_INDENT=20, WRAP_WIDTH=68 -> content width 48 (l.36).
- line 1: 8 sp + `release {releaseNumber} {filenames}` (releaseNumber is an opaque string, legacy non-numeric allowed; filenames not wrapped).
- then for each of `title`, `category`, `creator(s)`: value wrapped with greedy `wrapWords(value, 48, 48)` (`src/shared/text-wrap.ts`: split on whitespace, join with single space, a word longer than width stands alone, empty value -> `['']`). First line = 8 sp + label.padEnd(12) + wrapped[0]; continuations = 20 spaces + line. Empty value leaves the label line with trailing spaces (label padded to 12).
- The current release is NOT synthesised: `history[0]` is already the just-published release (comment l.58-61); empty history -> heading present, no entries (test nfo-generator.spec.ts l.135).
- `_releaseNumber` param is unused in renderNfo; the number reaches the NFO only via `history`.

Division switch: `generateOutputs` (l.134-164) calls `renderXInnovationsNfo` when `form.division === 'xinnovations'`, else `renderNfo`. DIZ is the same function for both.

## 2. /X Innovations NFO

Files: `src/main/nfo-x-innovations.ts` (renderer), `src/main/x-innovations-template.ts` (GENERATED constant, one JSON string), `scripts/make-x-template.py` (generator).

Relationship: the python script reads `test/fixtures/UP-MD15.NFO` (utf-8, errors=replace), splits on `\n`, repairs line index 35 (cut to 74 cols, rstrip, only if > 74), replaces 1-based inclusive line ranges with markers (make-x-template.py l.20-29), substitutes the 4 bracketed values on fixture lines 29/30, `json.dumps` the result into the .ts file. Regions: 26 -> `{{RELEASE_NAME_LINE}}`; 29/30 -> field lines with `{{DATE}} {{CODER}} {{SYSTEM}} {{DESIGN}}`; 39-87 `{{NOTES}}`; 94-98 `{{NEWS}}`; 107-113 `{{ROSTER}}`; 123-127 `{{BOARDS}}`; 138-254 `{{RELEASE_LIST}}`. Header of the .ts says "Regenerate with scripts/make-x-template.ts" but the script is `.py` (stale comment). Script writes with `open(OUT,'w')` without explicit encoding (unconfirmed effect; template is pure ASCII). Template = 10 markers, whole-line markers (`{{NOTES}}` etc. sit alone on a line) plus 4 inline bracket markers.

Substitution (`renderXInnovationsNfo`, l.308-322): sequential JS `String.replace(string, string)` - first occurrence only, and `$&`/`$'`/`` $` `` in the replacement text are interpreted by JS (unconfirmed by test; a `$` in notes/boards could corrupt output). Block markers handled by `fillBlock` (l.303-306): empty block -> the marker's whole line removed incl. its `\n` (regex `^marker\n?` multiline); otherwise replaced by `lines.join('\n')`.

Frame: each content line = `   | %% |` (9 cols) + 59-col field + `| %% |` (6) = 74 (`frameLine`, l.15-22; text padEnd(59) then slice(0,59) = clipped, never overflows). `ENTRY_RULE` = `- -| %% |` + 59 dashes + `| %% |- -` (77 cols, l.25).

Inline fields (l.288-293): DATE/CODER/SYSTEM/DESIGN each exactly 10 columns, `fitField` clips or pads right. (Template art is drawn for 10.)

Release name line (`releaseNameLine`, l.46-51): text `<<-- {name} -->>` clipped to 59, centred LEFT-heavy on odd remainder (`ceil(pad/2)` left), inside a frameLine. Empty block never removed (always present).

Notes (`wrapNotes`, l.62-84): width = 58 (FIELD_WIDTH-1; one col is the space after the gutter). CRLF -> LF; each line rtrimmed; a line <= 58 is emitted verbatim (leading spaces kept = hand-laid-out ASCII); a longer line is word-wrapped greedily keeping its own leading indent on every continuation. Each output line becomes `frameLine(' ' + line)`. Empty notes -> `wrapNotes('')` = `['']` -> one empty-ish framed line (not removed) (derived from code; unconfirmed by test).

News (`newsBlock`, l.247-263): split lines, each trimmed, leading/trailing blank lines dropped; empty -> `[]` (marker line removed). Else 2 blank frame lines, each line centred LEFT-heavy (ceil) in 59 (clipped, NOT wrapped), 2 blank frame lines. A multi-paragraph blank line inside news is kept as a centred empty line.

Roster (`rosterBlock`, l.96-125): group members by `role.trim()` in first-seen order (Map). Heading = `spacedCaps(role)`: each char separated by a space, first char and every `i` lower-case, others upper-case (`sysops` -> `s Y S O P S`; `artists` -> `a R T i S T S`). labelWidth = widest spaced label; row start = 2 spaces + label.padStart(labelWidth) (RIGHT-aligned labels) + ` >> `. Names joined by ` . `; if `line + piece` > 58 columns the line is flushed and the name continues on a new line indented by label length (hanging indent). One blank frameLine between groups. Names are not case-converted. Template provides 2 blank frame lines before and after the marker.

Boards (`boardsBlock`, l.147-167), fixed columns measured off the real file: name starts col 2 (0-based), sysop starts col 22 (BOARD_SYSOP_COL; dots fill up to col 21 then a space), HQ tag RIGHT-aligned to end at col 36, status RIGHT-aligned to end at col 56; gaps filled with `.`; each field surrounded by single spaces (`name ` ... ` sysop ` ... ` hq ` ... ` status`). Dots helper emits nothing if gap <= 0, so an overlong name pushes the row (then clipped by frameLine at 59). One row per board, order as given. Empty list -> marker line removed. Template then supplies a blank frame row and the fixed `https://www.uprough.net` row.

Release list (`releaseListBlock`, l.180-203): for each entry (newest first; current release prepended by ipc-handlers.ts l.157-168, display number from `formatReleaseNumber`): `ENTRY_RULE`, blank frame, `frameLine('  {displayNumber}:')`, each art line as `frameLine(7 spaces + artLine)` (fixed indent 7, not centred), 2 blank frames. After the loop: `ENTRY_RULE`, 2 blank frames (always, even with zero entries). Art per entry = `listArtFromBanner(dizText)` (l.215-221): the division's default banner with row 8 filled with the release name found by `releaseNameInBanner` (first non-decoration line after the `pRESENTS:|Presents:` line, frame chars `|\`'\/` stripped). So only the NAME, never the tagline. Centring inside the art uses `centerInSlot` (left-heavy). Documented fact: the 7 historical entries in the fixture are NOT reproducible by any rule (hand-drawn, differing centring/case), so the golden test feeds entries read from the fixture (x-golden.spec.ts l.29-47).

Centring conventions differ and are each pinned: `centerInField` (right-heavy, floor, l.31-36) is used by tests only (unconfirmed: it is exported; no production caller seen in this file); release-name line, news and DIZ slot use left-heavy (ceil).

Fixed template remainder: logo, `p R E S E N T S`, section heading art, URL row, closing art, footer credit `nFO bY diPSWiTCH...`, and the tail `\n\n\n\n\n\n` (file ends with 5 blank lines after the last art line, matching fixture).

Number format: `formatReleaseNumber` (divisions.ts l.111-114): UP plain; /X zero-padded to 3 with `0` -> `o` (`oo7`).

## 3. FILE_ID.DIZ

Writer: `renderDiz(form)` = `toAsciiSafe(trimDizBanner(form.dizBannerText)) + '\n'` (nfo-generator.ts l.115-124). `trimDizBanner`: rtrim spaces/tabs per line, drop leading and trailing blank rows, keep LEADING spaces. Also used for the catalogue copy (releases-client.ts l.129).

Banner inheritance (renderer/main.ts l.101-170, divisions.ts):
- `Division.inheritsBannerArt`: Up Rough `true` (template = previous release's `SeedState.currentDizText` from `loadSeedState`/`fetchSeed`, ipc-handlers.ts l.70-92; falls back to `defaultDizBanner` when empty); /X `false` (always its own `defaultDizBanner`, because old banners ran 11-13 rows).
- `defaultDizBanner`: 10 rows x 45 cols (`UP_ROUGH_BANNER`, `X_INNOVATIONS_BANNER`, divisions.ts l.53-84). Identical except row 7 (0-based): UR `Up Rough Presents:`, /X `uP rOUGH /X iNNOVATiONS pRESENTS:`. Rows 8 blank interior, row 9 closing rule. Note: the max of 10 rows x 45 columns is in comments only (divisions.ts l.66-70, diz-release-line.ts l.11); no enforcement code found (unconfirmed).
- `DizRowSpec {row,label,placeholder}`; `editableDizRows` for BOTH divisions is a single row `{row: 8, label 'Release line'}` (UR placeholder `HYPOCRISY - A500 HDD DEMO FROM CS 2025`, /X `mORE dOORS cURSOR mENU 1.5`). The /X plan doc (l.84-103) wanted two rows plus a tagline; code has one. The shipped fixture DIZ (`test/fixtures/UP-MD15-FILE_ID.DIZ`) has 12 rows, with a tagline on row 10 and the name on row 9 (legacy, taller than the tool draws).
- Release line composition (`src/shared/diz-release-line.ts`): `findReleaseLineSlot(banner,row)` rtrim the row, FRAME_HEAD `^[^\sA-Za-z0-9]{1,3}` and FRAME_TAIL `[^\sA-Za-z0-9]{1,3}$` define the writable span [start,end) (start = head length, end = tail start; if no tail or empty span, end=line length); row clamped to art height. `applyReleaseLineText`: rtrim row, padEnd to slot.end, replace interior with `centerInSlot(text,width)`: trim, clip to width (truncates, no wrap), left pad = ceil(pad/2) (left-heavy), right fills. `applyReleaseLines` applies several rows independently. Frame characters on the line survive. For the default rows the width is 42 (slot 1..43).
- Renderer preview/char counter use the same slot (main.ts l.108-130).
- Transliteration: `toAsciiSafe` maps ä ö å ü é è ê Ä Ö Å Ü É ñ ç ø Ø æ Æ to ASCII digraphs, any other non-ASCII -> `?`. The NFO is NOT run through it.

## 4. Encoding and line ends

- Writing: `staging.ts` l.15-16 `writeLatin1(file_id.diz, dizText)` and `writeLatin1(UP-{code}.NFO, nfoText)`; `encoding.ts`: `assertLatin1Safe` throws on any code point > 0xFF (names char and position), then `Buffer.from(text,'latin1')`. So output = Latin-1 bytes (DIZ is effectively pure ASCII after toAsciiSafe; NFO may carry 0x80-0xFF if typed).
- Line ends: LF only everywhere in generators. Fixtures contain 0 CR bytes (grep -c). wrapNotes and newsBlock convert incoming CRLF to LF; the UR path (`indentBlock`) does not (unconfirmed upstream normalisation).
- Trailing spaces: UR NFO keeps them where produced (BANNER last art line, history label lines with empty values, padEnd'd member lines when role empty is n/a). /X lines are full width by padding (frame). DIZ: all trailing whitespace stripped.
- Final newline: UR NFO ends with exactly one `\n`; /X NFO ends with the template tail (5 blank lines); DIZ ends with one `\n`.
- Golden tests: `test/unit/x-golden.spec.ts` reads fixture as latin1, splits on `\n`, renders with the fixture's values and compares LINE BY LINE (selected lines by index, selected lines via `toContain`) and finally asserts the set of differing line indices equals exactly `[35]` (the 283-col paste-accident line; fixture 283 chars, output 74) plus equal line count. This is effectively byte-exact except that single line. It also asserts every line starting `   | %% |` is 74 wide. `nfo-x-innovations.spec.ts` pins sub-pieces (frameLine == fixture[36], ENTRY_RULE == fixture[137], wrapNotes, roster, boards rows 123-127 vs fixture, no bare lines, empty input keeps widths). Up Rough NFO has NO fixture and NO golden test: `nfo-generator.spec.ts` uses `toContain` substring checks and member/history unit tests; DIZ tests are string equality on small inputs. A real Up Rough NFO is not in `test/fixtures` (only UP-MD15.NFO, which is /X). The DIZ fixture is only referenced by door-repo-client.spec.ts (not a generator golden).
- Gap for parity: Up Rough parity currently needs a golden file that does not exist yet (unconfirmed whether an old UR NFO exists elsewhere in the repo; not searched beyond app/test).

## 5. Variable inventory

Form field = renderer input; server = seed/manifest from releases server; computed = derived in code.

| Variable | Source | NFO rule | DIZ rule | Divisions |
|----------|--------|----------|----------|-----------|
| releaseName | form (UR) | `Release named {x}` | via release line text (separate input) | UR |
| entryTitle | form | /X: name in `<<-- x -->>` (59, ceil-centre); UR: history title | - | both |
| platformTag | form | ` ({x})` if non-empty | - | UR |
| code, format | form | `(UP-{CODE upper}.{format})` | - | UR (filename also via staging for both) |
| releaseDate | form (UR defaults `todayIso()` main.ts l.382; /X has own input `x-release-date`) | UR: `Released {x}` line, omitted if empty; /X: 10-col bracket field | - | both |
| coder / design | computed from credits by role (main.ts l.180-181, 493) | /X: 10-col bracket fields | - | /X |
| system | form | /X: 10-col field | - | /X |
| releaseNotesText | form | UR: 8-space indent, no wrap; /X: framed, 58 wrap-if-over, indent kept | - | both |
| newsText | form | UR: 8-space indent, no wrap; /X: trimmed, centred, clipped 59, 2+2 blank frames, section removed if empty | - | both |
| members[{nickname,role}] | server seed, editable | UR: one line, nickname padEnd(max(20,len+2)); /X: grouped by role, spaced-caps right-aligned label, ` . ` join, wrap 58 hanging indent | - | both |
| boards[{name,sysop,hq,status}] | server seed (`boards`) | /X only, fixed-column dotted table, right-aligned HQ/status | - | /X |
| history[{releaseNumber,filenames,title,category,creators}] | server | UR: block per entry, blank-line separated, 48-col wrap of 3 labelled fields, 20-sp continuation | - | UR |
| releaseNumber | server / computed (`formatReleaseNumber`) | UR: in history[0]; /X: display number (`oo7`) in list | - | both |
| releaseList[{displayNumber,art}] | server manifest + computed (`listArtFromBanner`) | /X: repeated banner block, indent 7 | - | /X |
| dizBannerText | computed (inherited template + release line) | /X list art derives from it (name only) | whole file, trimmed | both |
| release line text | form (editableDizRows[0]) | - | centre-left-heavy in slot (width 42), clipped | both |
| (fixed text) | constant | headings, URL, footer | rows 0-7, 9 | both |

## 6. Shared versus different (facts)

Shared: `generateOutputs` entry point and `GeneratedOutputs`; `renderDiz`/`trimDizBanner` for both; `members` list; notes and news text fields; release date; `writeLatin1` and staging; LF endings; left-heavy ceil centring in DIZ slot, name line and news; the frame/art are fixed strings held in constants (UR in `constants.ts`, /X in a generated template).

Different:
- UR NFO is built by code concatenation of constants (no template), 8-space indent, no width enforcement except the history 48-col wrap. /X NFO is a template with 10 markers plus a code-built 74-col frame; every content line is exactly 74 columns and clipped.
- UR text blocks are unwrapped and unclipped; /X notes wrap at 58 only when over, news clipped at 59, never wrapped.
- UR members are a 2-column aligned list; /X members are grouped by role with right-aligned spaced-caps labels and ` . ` wrapping.
- UR has history text entries (wrapWords); /X has the release list of embedded banners plus a boards table.
- Conditional line: UR date line vanishes if empty; /X sections vanish (news, boards, roster) if empty but notes and release list do not.
- Banner inheritance: UR inherits previous DIZ art; /X always its default.
- `wrapWords` (text-wrap.ts) is shared by UR history and the Aminet readme (`aminet-readme.ts` l.3, 96, 127: width AMINET_LINE_MAX 78); /X `wrapNotes` is a separate implementation (keeps indent, keeps short lines verbatim) - not shared.
- Template numbering: the .ts template says generated by `make-x-template.ts`, repo script is `.py`.
