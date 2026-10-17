/**
 * The group releases under two names, and everything that differs between
 * them is described here rather than decided at each call site. A renderer
 * or generator that needs to know "which art, which lines, what is this
 * release called" reads a Division record; it never asks which division it
 * is and branches.
 *
 * The server owns the numbering (see the releases server's `division`
 * column); these ids are the strings it accepts.
 */
export type DivisionId = 'uprough' | 'xinnovations';

export const DEFAULT_DIVISION: DivisionId = 'uprough';

export interface Division {
  id: DivisionId;
  /** What the radio button says. */
  label: string;
  /**
   * The banner art a release inherits when the division has published
   * nothing yet. Both divisions draw the same logo; they differ in how many
   * text rows sit under it.
   */
  defaultDizBanner: string;
  /**
   * Rows of the banner art anybody may retype, zero-based, in the order the
   * form shows them. Everything else in the art is inherited verbatim -
   * hand-editing the grid is how it got mangled before.
   */
  editableDizRows: DizRowSpec[];
  /** Whether releases of this division go to the AmiExpress door repository. */
  submitsToDoorRepo: boolean;
  /**
   * Whether a release inherits the previous one's banner art.
   *
   * Up Rough's art is redrawn over the years and each release carries the
   * last one's, so nobody has to retype a 45-column drawing. /X's is a fixed
   * ten-row template owned here: inheriting it would mean building on art of
   * whatever height the last release happened to ship (they range from 11 to
   * 13 rows), and the editable row would no longer be where the division
   * says it is.
   */
  inheritsBannerArt: boolean;
}

export interface DizRowSpec {
  row: number;
  /** Field label in the form, e.g. "Release line" or "Tagline". */
  label: string;
  placeholder: string;
}

const UP_ROUGH_BANNER = [
  '    _ _ __ ___    ____  _ _ __ _________',
  '   | | Y  / _ \\../ __ \\| | Y  / __  | | |',
  'dNo| | | | | | || /_ | | | | | |_ | l | |dS!',
  '|  | | | | |`| || || | | | | | || |___| |  |',
  "|  | | l | |__/'| || | | | | | || |   | |  |",
  '|  | l___| |    | || | | l l | ll | | | |  |',
  "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |",
  '|            Up Rough Presents:            |',
  '|                                          |',
  "`------------------------------------------'",
].join('\n');

// Same logo and the same ten lines as Up Rough's, because that is what
// FILE_ID.DIZ is allowed to be: the format is capped at ten rows of 45
// columns, and a taller one gets truncated or refused by BBS software. The
// /X releases shipped 11 to 13 rows for years, which is why this one is
// shorter than the art in the MD15 release - the division's name takes the row Up
// Rough uses for "Up Rough Presents:", and the release names itself on the
// next.
const X_INNOVATIONS_BANNER = [
  '    _ _ __ ___    ____  _ _ __ _________',
  '   | | Y  / _ \\../ __ \\| | Y  / __  | | |',
  'dNo| | | | | | || /_ | | | | | |_ | l | |dS!',
  '|  | | | | |`| || || | | | | | || |___| |  |',
  "|  | | l | |__/'| || | | | | | || |   | |  |",
  '|  | l___| |    | || | | l l | ll | | | |  |',
  "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |",
  '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
  '|                                          |',
  "`------------------------------------------'",
].join('\n');

export const DIVISIONS: Record<DivisionId, Division> = {
  uprough: {
    id: 'uprough',
    label: 'Up Rough',
    defaultDizBanner: UP_ROUGH_BANNER,
    editableDizRows: [{ row: 8, label: 'Release line', placeholder: 'HYPOCRISY - A500 HDD DEMO FROM CS 2025' }],
    submitsToDoorRepo: false,
    inheritsBannerArt: true,
  },
  xinnovations: {
    id: 'xinnovations',
    label: 'Up Rough /X Innovations',
    defaultDizBanner: X_INNOVATIONS_BANNER,
    editableDizRows: [{ row: 8, label: 'Release line', placeholder: 'mORE dOORS cURSOR mENU 1.5' }],
    submitsToDoorRepo: true,
    inheritsBannerArt: false,
  },
};

/**
 * How a division writes a release number. Up Rough writes it plainly; /X uses
 * the group's old typography, three wide with the letter o for zero, so 7 is
 * "oo7". Must agree with the releases server's formatReleaseNumber - the
 * server stores the label, this is for previewing it before the build.
 */
export function formatReleaseNumber(releaseNumber: number, division: DivisionId): string {
  if (division !== 'xinnovations') return String(releaseNumber);
  return String(releaseNumber).padStart(3, '0').replace(/0/g, 'o');
}

export function isDivisionId(value: unknown): value is DivisionId {
  return value === 'uprough' || value === 'xinnovations';
}

export function divisionOf(id: DivisionId): Division {
  return DIVISIONS[id];
}
