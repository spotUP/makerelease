import { DIVISIONS } from './divisions.js';

// The row to work on when a caller does not name one. It belongs to a
// division, not to this module - Up Rough's is the historical default, and
// every division that has more than one editable row passes them explicitly.
const DEFAULT_ROW = DIVISIONS.uprough.editableDizRows[0].row;

// The file_id.diz banner art is fixed: it is inherited verbatim from the last
// published release (SeedState.currentDizText, falling back to
// DEFAULT_DIZ_BANNER) and the release maker deliberately gives nobody a way to
// retype it - hand-editing the 45x10 grid is how the art got mangled before.
// Exactly one line changes per release, the one naming the release, and this
// module is the only code that writes it.
//
// The writable span of that line is whatever sits between its frame
// characters, so the '|' borders (or whatever the art uses) survive every
// edit and the text is re-centred inside them.

/** Leading/trailing frame run: up to three punctuation chars, e.g. '|' or '.-'. */
const FRAME_HEAD = /^[^\sA-Za-z0-9]{1,3}/;
const FRAME_TAIL = /[^\sA-Za-z0-9]{1,3}$/;

export interface ReleaseLineSlot {
  /** Row the release line lives on, clamped to the art's real height. */
  row: number;
  /** First writable column, inclusive - just inside the left frame. */
  start: number;
  /** Last writable column, exclusive - just inside the right frame. */
  end: number;
  /** How many characters fit between the frames. */
  width: number;
}

function rtrim(line: string): string {
  return line.replace(/[ \t]+$/, '');
}

export function findReleaseLineSlot(banner: string, row: number = DEFAULT_ROW): ReleaseLineSlot {
  const lines = banner.split('\n');
  const clampedRow = Math.max(0, Math.min(lines.length - 1, row));
  const line = rtrim(lines[clampedRow] ?? '');
  const head = FRAME_HEAD.exec(line);
  const tail = FRAME_TAIL.exec(line);
  const start = head ? head[0].length : 0;
  const tailStart = tail ? line.length - tail[0].length : line.length;
  const end = tailStart > start ? tailStart : line.length;
  return { row: clampedRow, start, end, width: Math.max(0, end - start) };
}

/** The release text currently sitting in the art, without its centring spaces. */
export function readReleaseLineText(banner: string, row: number = DEFAULT_ROW): string {
  const slot = findReleaseLineSlot(banner, row);
  const line = rtrim(banner.split('\n')[slot.row] ?? '');
  return line.slice(slot.start, slot.end).trim();
}

/**
 * Centres `text` inside the slot, clipping anything that does not fit.
 *
 * Left-heavy on an odd remainder, which is how the group centres everything
 * they draw by hand - the release-name line, the group-news line and all
 * seven banners embedded in the /X release list agree on it.
 */
export function centerInSlot(text: string, width: number): string {
  const clipped = text.trim().slice(0, width);
  const pad = width - clipped.length;
  const left = Math.ceil(pad / 2);
  return ' '.repeat(left) + clipped + ' '.repeat(pad - left);
}

/** Returns the whole banner with only the release line's interior replaced. */
export function applyReleaseLineText(banner: string, text: string, row: number = DEFAULT_ROW): string {
  const slot = findReleaseLineSlot(banner, row);
  const lines = banner.split('\n');
  const line = rtrim(lines[slot.row] ?? '').padEnd(slot.end, ' ');
  lines[slot.row] = line.slice(0, slot.start) + centerInSlot(text, slot.width) + line.slice(slot.end);
  return lines.join('\n');
}

/**
 * Applies several release lines at once, which is what a division with more
 * than one editable row needs: /X names the release on one row and carries a
 * tagline on the next. Rows are independent - each is centred in its own
 * slot - so the order they are applied in does not matter.
 */
export function applyReleaseLines(banner: string, lines: ReadonlyArray<{ row: number; text: string }>): string {
  return lines.reduce((art, line) => applyReleaseLineText(art, line.text, line.row), banner);
}

/** The text currently sitting in each of the given rows. */
export function readReleaseLines(banner: string, rows: readonly number[]): string[] {
  return rows.map((row) => readReleaseLineText(banner, row));
}
