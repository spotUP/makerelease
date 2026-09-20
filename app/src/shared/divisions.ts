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

// Same logo, three text rows instead of two: the division's name is fixed on
// the first, and a release fills in the name and a tagline under it. Taken
// from UP-MD15's FILE_ID.DIZ.
const X_INNOVATIONS_BANNER = [
  '    _ _ __ ___    ____  _ _ __ _________',
  '   | | Y  / _ \\../ __ \\| | Y  / __  | | |',
  'dNo| | | | | | || /_ | | | | | |_ | l | |dS!',
  '|  | | | | |`| || || | | | | | || |___| |  |',
  "|  | | l | |__/'| || | | | | | || |   | |  |",
  '|  | l___| |    | || | | l l | ll | | | |  |',
  "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |",
  '|                                          |',
  '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
  '|                                          |',
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
  },
  xinnovations: {
    id: 'xinnovations',
    label: 'Up Rough /X Innovations',
    defaultDizBanner: X_INNOVATIONS_BANNER,
    editableDizRows: [
      { row: 9, label: 'Release line', placeholder: 'mORE dOORS cURSOR mENU 1.5' },
      { row: 10, label: 'Tagline', placeholder: 'uPDATES 4 A bETTER eXPERIENCE!' },
    ],
    submitsToDoorRepo: true,
  },
};

export function isDivisionId(value: unknown): value is DivisionId {
  return value === 'uprough' || value === 'xinnovations';
}

export function divisionOf(id: DivisionId): Division {
  return DIVISIONS[id];
}
