import { BoardEntry, MemberEntry } from '../shared/types';
import { DIVISIONS } from '../shared/divisions';
import { applyReleaseLines } from '../shared/diz-release-line';
import { X_INNOVATIONS_NFO_TEMPLATE } from './x-innovations-template';

/**
 * The /X Innovations NFO is a fixed drawing with a few regions a release
 * fills in. Everything here is about fitting text into that drawing without
 * disturbing it: each content line is
 *
 *     "   | %% |" + 59 columns of field + "| %% |"
 *
 * and a line that is one column wrong is a visibly broken frame.
 */
const GUTTER_LEFT = '   | %% |';
const GUTTER_RIGHT = '| %% |';
export const FIELD_WIDTH = 59;

/** A content line: text placed in the field, padded to the frame. */
export function frameLine(text = ''): string {
  return GUTTER_LEFT + text.padEnd(FIELD_WIDTH, ' ').slice(0, FIELD_WIDTH) + GUTTER_RIGHT;
}

/** The rule that separates release-list entries, as drawn in the original. */
export const ENTRY_RULE = `- -| %% |${'-'.repeat(FIELD_WIDTH)}| %% |- -`;

/**
 * Centres in the field, leaning RIGHT-heavy on an odd remainder, which is how
 * all seven embedded banners in the original are drawn.
 */
export function centerInField(text: string): string {
  const clipped = text.slice(0, FIELD_WIDTH);
  const pad = FIELD_WIDTH - clipped.length;
  const left = Math.floor(pad / 2);
  return ' '.repeat(left) + clipped + ' '.repeat(pad - left);
}

/**
 * The line that names the release, drawn as "<<-- name -->>".
 *
 * Centred the other way round from the banners above - left-heavy on an odd
 * remainder. The two were drawn by hand and simply do not agree; each is
 * pinned by its own assertion against the real file rather than averaged into
 * one rule that matches neither.
 */
export function releaseNameLine(releaseName: string): string {
  const text = `<<-- ${releaseName} -->>`.slice(0, FIELD_WIDTH);
  const pad = FIELD_WIDTH - text.length;
  const left = Math.ceil(pad / 2);
  return frameLine(' '.repeat(left) + text + ' '.repeat(pad - left));
}

/**
 * Wraps prose into the field, keeping the author's own line breaks. The notes
 * are hand-laid-out ASCII ("Description", a rule of dashes under it, an
 * indented list), so a line that already fits is left exactly as typed and
 * only over-long ones are broken.
 */
// One column is spent on the space that sets the notes in from the gutter,
// so a note line has FIELD_WIDTH - 1 to live in. Taking one column too few
// wraps a line that fitted in the original and grows the whole NFO by a row.
export function wrapNotes(notes: string, width = FIELD_WIDTH - 1): string[] {
  const out: string[] = [];
  for (const raw of notes.replace(/\r\n/g, '\n').split('\n')) {
    const line = raw.replace(/\s+$/, '');
    if (line.length <= width) {
      out.push(line);
      continue;
    }
    const indent = (line.match(/^\s*/) ?? [''])[0];
    let current = '';
    for (const word of line.trim().split(/\s+/)) {
      const candidate = current === '' ? indent + word : `${current} ${word}`;
      if (candidate.length > width && current !== '') {
        out.push(current);
        current = indent + word;
      } else {
        current = candidate;
      }
    }
    if (current !== '') out.push(current);
  }
  return out;
}

/** " mORE dOORS 1.5 (aMiEXPRESS)" etc - notes sit one space in from the gutter. */
function notesBlock(notes: string): string[] {
  return wrapNotes(notes).map((line) => frameLine(` ${line}`));
}

/**
 * The roster, grouped the way the NFO prints it: a heading of spaced capitals,
 * then names separated by " . ", continuing on aligned lines when a group does
 * not fit. Group order is the order the roles were entered.
 */
export function rosterBlock(members: MemberEntry[]): string[] {
  const groups = new Map<string, string[]>();
  for (const member of members) {
    const key = member.role.trim();
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key)?.push(member.nickname);
  }

  const labelWidth = Math.max(...[...groups.keys()].map((role) => spacedCaps(role).length), 0);
  const out: string[] = [];
  let first = true;
  for (const [role, names] of groups) {
    if (!first) out.push(frameLine());
    first = false;
    const label = `  ${spacedCaps(role).padStart(labelWidth)} >> `;
    const continuation = ' '.repeat(label.length);
    let line = label;
    for (const [i, name] of names.entries()) {
      const piece = i === 0 ? name : ` . ${name}`;
      if ((line + piece).length > FIELD_WIDTH - 1) {
        out.push(frameLine(line));
        line = continuation + name;
      } else {
        line += piece;
      }
    }
    out.push(frameLine(line));
  }
  return out;
}

/**
 * "sysops" -> "s Y S O P S": the group's own lettering, where the first
 * character is lower case and so is every i - the same style as diPSWiTCH
 * and iNNOVATiONS. The real NFO prints "a R T i S T S", not "A R T I S T S".
 */
export function spacedCaps(role: string): string {
  const letters = role
    .trim()
    .split('')
    .map((c, i) => (c.toLowerCase() === 'i' || i === 0 ? c.toLowerCase() : c.toUpperCase()));
  return letters.join(' ');
}

/**
 * The boards table, on the fixed columns the original is drawn on: the board
 * name starts at column 2, the sysop at 22, the HQ tag ends at 36 and the
 * status ends at 56, with dots filling every gap. Measured off the real file
 * rather than guessed - the HQ column is right-aligned, which no amount of
 * looking at one row would have told you.
 */
const BOARD_NAME_COL = 2;
const BOARD_SYSOP_COL = 22;
const BOARD_HQ_END = 36;
const BOARD_STATUS_END = 56;

function dotsBetween(from: number, to: number): string {
  return to > from ? '.'.repeat(to - from) : '';
}

export function boardsBlock(boards: BoardEntry[]): string[] {
  return boards.map((board) => {
    let line = ' '.repeat(BOARD_NAME_COL) + board.name + ' ';
    line += dotsBetween(line.length, BOARD_SYSOP_COL - 1) + ' ';
    line += board.sysop + ' ';
    const hqStart = BOARD_HQ_END - board.hq.length;
    line += dotsBetween(line.length, hqStart - 1) + ' ' + board.hq + ' ';
    const statusStart = BOARD_STATUS_END - board.status.length;
    line += dotsBetween(line.length, statusStart - 1) + ' ' + board.status;
    return frameLine(line);
  });
}

/**
 * The release list: every release so far, newest first, each as its own
 * banner art indented inside the frame under its number.
 *
 * Only the release's NAME goes into the embedded art, never a tagline -
 * every entry in the real NFO carries the fixed "pRESENTS:" line and one
 * name line, including releases whose own FILE_ID.DIZ has more.
 */
/** Where an embedded banner starts inside the field, measured off the file. */
const LIST_ART_INDENT = 7;

export function releaseListBlock(
  entries: ReadonlyArray<{ displayNumber: string; art: string }>,
): string[] {
  const out: string[] = [];
  for (const entry of entries) {
    out.push(ENTRY_RULE);
    out.push(frameLine());
    out.push(frameLine(`  ${entry.displayNumber}:`));
    for (const artLine of entry.art.split('\n')) {
      // Indented, not centred: the whole block sits at a fixed column so the
      // art keeps its own shape. Centring each line separately makes the
      // short rows of the logo drift against the long ones.
      out.push(frameLine(' '.repeat(LIST_ART_INDENT) + artLine));
    }
    out.push(frameLine());
    out.push(frameLine());
  }
  // The list closes with a rule of its own, the way the original does, so
  // the section ends rather than running into the footer art.
  out.push(ENTRY_RULE);
  out.push(frameLine());
  out.push(frameLine());
  return out;
}

/**
 * The art a release shows in the release list, built from the banner it
 * shipped.
 *
 * Only the name survives: every entry in the real NFO carries the fixed
 * "pRESENTS:" line and one name line, even for releases whose own
 * FILE_ID.DIZ also has a tagline. So the list is drawn from the division's
 * own art with just that one row filled in, which also keeps the seven
 * historical entries and every future one identical in shape.
 */
export function listArtFromBanner(banner: string): string {
  const division = DIVISIONS.xinnovations;
  const [nameRow] = division.editableDizRows;
  return applyReleaseLines(division.defaultDizBanner, [
    { row: nameRow.row, text: releaseNameInBanner(banner) },
  ]);
}

/**
 * The release's name as its banner gives it: the first line with anything on
 * it below the one announcing the division.
 *
 * Found by the announcement rather than by row number, because the banners
 * this reads are not all the same height. The seven releases that predate
 * this tool ship 11, 12 and 13-row art with the name in a different place in
 * each, and a fixed index would read a blank row or the frame out of them.
 */
export function releaseNameInBanner(banner: string): string {
  const lines = banner.split('\n');
  const announced = lines.findIndex((line) => /pRESENTS:|Presents:/i.test(line));
  for (const line of lines.slice(announced + 1)) {
    const text = line.replace(/^[|`'\\/]+|[|`'\\/]+$/g, '').trim();
    if (text && !/^[-_=.\s]+$/.test(text)) return text;
  }
  return '';
}

/**
 * The group-news block under its own drawn heading. The original sets one
 * centred line between two blank rows on each side, which is the shape this
 * keeps however many lines the news runs to.
 */
export function newsBlock(news: string): string[] {
  const lines = news
    .replace(/\r\n/g, '\n')
    .split('\n')
    .map((line) => line.trim());
  while (lines.length > 0 && lines[0] === '') lines.shift();
  while (lines.length > 0 && lines[lines.length - 1] === '') lines.pop();
  if (lines.length === 0) return [];
  // Centred left-heavy, like the release-name line and unlike the banners.
  const centred = (line: string): string => {
    const text = line.slice(0, FIELD_WIDTH);
    const pad = FIELD_WIDTH - text.length;
    const left = Math.ceil(pad / 2);
    return ' '.repeat(left) + text + ' '.repeat(pad - left);
  };
  return [frameLine(), frameLine(), ...lines.map((line) => frameLine(centred(line))), frameLine(), frameLine()];
}

export interface XInnovationsNfoInput {
  releaseName: string;
  releaseDate: string;
  coder: string;
  system: string;
  design: string;
  releaseNotes: string;
  news: string;
  members: MemberEntry[];
  boards: BoardEntry[];
  /** Newest first, each with the banner art its list entry shows. */
  releaseList: ReadonlyArray<{ displayNumber: string; art: string }>;
}

/**
 * A bracketed field keeps the drawing's width: the art around it was drawn
 * for a value of exactly that many columns, so a longer one is clipped and a
 * shorter one padded rather than shoving the frame sideways.
 */
function fitField(value: string, width: number): string {
  return value.length >= width ? value.slice(0, width) : value.padEnd(width, ' ');
}

const FIELD_WIDTHS = {
  DATE: 10,
  CODER: 10,
  SYSTEM: 10,
  DESIGN: 10,
} as const;

/**
 * Fills a marker with the lines of a block, or removes the marker's line
 * entirely when the block is empty.
 *
 * An empty block used to leave the marker as an empty string, which is a
 * bare line with no frame on it - a visible gap torn through the middle of
 * the drawing. A section with nothing in it should simply not take up a row.
 */
function fillBlock(template: string, marker: string, lines: string[]): string {
  if (lines.length === 0) return template.replace(new RegExp(`^${marker}\\n?`, 'm'), '');
  return template.replace(marker, () => lines.join('\n'));
}

export function renderXInnovationsNfo(input: XInnovationsNfoInput): string {
  const filled = X_INNOVATIONS_NFO_TEMPLATE.replace('{{RELEASE_NAME_LINE}}', () => releaseNameLine(input.releaseName))
    .replace('{{DATE}}', () => fitField(input.releaseDate, FIELD_WIDTHS.DATE))
    .replace('{{CODER}}', () => fitField(input.coder, FIELD_WIDTHS.CODER))
    .replace('{{SYSTEM}}', () => fitField(input.system, FIELD_WIDTHS.SYSTEM))
    .replace('{{DESIGN}}', () => fitField(input.design, FIELD_WIDTHS.DESIGN));

  return [
    ['{{NOTES}}', notesBlock(input.releaseNotes)],
    ['{{NEWS}}', newsBlock(input.news)],
    ['{{ROSTER}}', rosterBlock(input.members)],
    ['{{BOARDS}}', boardsBlock(input.boards)],
    ['{{RELEASE_LIST}}', releaseListBlock(input.releaseList)],
  ].reduce<string>((art, [marker, lines]) => fillBlock(art, marker as string, lines as string[]), filled);
}
