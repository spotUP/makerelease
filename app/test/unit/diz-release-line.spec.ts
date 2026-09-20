import { describe, expect, it } from 'vitest';
import {
  applyReleaseLineText,
  centerInSlot,
  findReleaseLineSlot,
  readReleaseLineText, applyReleaseLines, readReleaseLines } from '../../src/shared/diz-release-line';
import { DIVISIONS } from '../../src/shared/divisions';

const DEFAULT_DIZ_BANNER = DIVISIONS.uprough.defaultDizBanner;
const DIZ_RELEASE_LINE_ROW = DIVISIONS.uprough.editableDizRows[0].row;

const HYPOC_BANNER = [
  '    _ _ __ ___    ____  _ _ __ _________',
  '   | | Y  / _ \\../ __ \\| | Y  / __  | | |',
  'dNo| | | | | | || /_ | | | | | |_ | l | |dS!',
  '|  | | | | |`| || || | | | | | || |___| |  |',
  "|  | | l | |__/'| || | | | | | || |   | |  |",
  '|  | l___| |    | || | | l l | ll | | | |  |',
  "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |",
  '|            Up Rough Presents:            |',
  '|  HYPOCRISY - A500 HDD DEMO FROM CS 2025  |',
  "`------------------------------------------'",
].join('\n');

const rowOf = (banner: string, row = DIZ_RELEASE_LINE_ROW): string => banner.split('\n')[row];

describe('findReleaseLineSlot', () => {
  it('puts the writable span between the release line frame characters', () => {
    const slot = findReleaseLineSlot(HYPOC_BANNER);
    expect(slot.row).toBe(8);
    expect(slot.start).toBe(1);
    expect(slot.end).toBe(43);
    expect(slot.width).toBe(42);
  });

  it('clamps to the last row when the art is shorter than the release row', () => {
    const slot = findReleaseLineSlot('|  only line  |');
    expect(slot.row).toBe(0);
    expect(slot.width).toBe(13);
  });

  it('treats a line with no frame characters as writable end to end', () => {
    const slot = findReleaseLineSlot('a release line', 0);
    expect(slot.start).toBe(0);
    expect(slot.end).toBe(14);
  });
});

describe('readReleaseLineText', () => {
  it('reads the release text without its centring spaces', () => {
    expect(readReleaseLineText(HYPOC_BANNER)).toBe('HYPOCRISY - A500 HDD DEMO FROM CS 2025');
  });

  it('reads an empty string from blank art', () => {
    expect(readReleaseLineText(DEFAULT_DIZ_BANNER)).toBe('');
  });
});

describe('centerInSlot', () => {
  it('centres short text and pads the slot to its full width', () => {
    expect(centerInSlot('abc', 9)).toBe('   abc   ');
  });

  it('puts the odd leftover space on the right of the text, as the group draws it', () => {
    // Left-heavy: the release-name line, the group-news line and all seven
    // banners in the /X release list are drawn this way.
    expect(centerInSlot('abc', 8)).toBe('   abc  ');
  });

  it('hard-cuts text longer than the slot', () => {
    expect(centerInSlot('abcdefgh', 4)).toBe('abcd');
  });
});

describe('applyReleaseLineText', () => {
  it('replaces only the release line and keeps every other row byte for byte', () => {
    const updated = applyReleaseLineText(HYPOC_BANNER, 'NEW RELEASE');
    const before = HYPOC_BANNER.split('\n');
    const after = updated.split('\n');
    expect(after.length).toBe(before.length);
    after.forEach((line, i) => {
      if (i !== DIZ_RELEASE_LINE_ROW) expect(line).toBe(before[i]);
    });
    expect(rowOf(updated)).toBe('|                NEW RELEASE               |');
  });

  it('keeps the pipes intact when the text is longer than the gap between them', () => {
    const tooLong = 'X'.repeat(120);
    const line = rowOf(applyReleaseLineText(HYPOC_BANNER, tooLong));
    expect(line.length).toBe(44);
    expect(line.startsWith('|')).toBe(true);
    expect(line.endsWith('|')).toBe(true);
    expect(line.slice(1, 43)).toBe('X'.repeat(42));
  });

  it('never lets the release line grow the art wider than it was', () => {
    for (const text of ['', 'a', 'a'.repeat(41), 'a'.repeat(42), 'a'.repeat(43)]) {
      const updated = applyReleaseLineText(HYPOC_BANNER, text);
      expect(rowOf(updated).length).toBe(44);
    }
  });

  it('writes into blank fallback art without disturbing its frame', () => {
    const line = rowOf(applyReleaseLineText(DEFAULT_DIZ_BANNER, 'UP-TEST'));
    expect(line.startsWith('|')).toBe(true);
    expect(line.endsWith('|')).toBe(true);
    expect(line.trim().slice(1, -1).trim()).toBe('UP-TEST');
  });

  it('round-trips: what is written is what is read back', () => {
    const updated = applyReleaseLineText(HYPOC_BANNER, 'SOME DEMO FROM SOME PARTY 2026');
    expect(readReleaseLineText(updated)).toBe('SOME DEMO FROM SOME PARTY 2026');
  });
});

describe('applyReleaseLines', () => {
  // The /X banner: the division name is fixed on row 9, the release names
  // itself on row 10 and adds a tagline on row 11.
  const X_BANNER = [
    '|                                          |',
    '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
    '|                                          |',
    '|                                          |',
    "`------------------------------------------'",
  ].join('\n');

  it('fills in every editable row, leaving the fixed ones alone', () => {
    const out = applyReleaseLines(X_BANNER, [
      { row: 2, text: 'mORE dOORS cURSOR mENU 1.5' },
      { row: 3, text: 'uPDATES 4 A bETTER eXPERIENCE!' },
    ]).split('\n');

    expect(out[1]).toBe('|     uP rOUGH /X iNNOVATiONS pRESENTS:    |');
    expect(out[2]).toContain('mORE dOORS cURSOR mENU 1.5');
    expect(out[3]).toContain('uPDATES 4 A bETTER eXPERIENCE!');
    expect(out[4]).toBe("`------------------------------------------'");
  });

  it('keeps the frame intact on every row it writes', () => {
    const out = applyReleaseLines(X_BANNER, [
      { row: 2, text: 'x'.repeat(200) },
      { row: 3, text: 'y'.repeat(200) },
    ]).split('\n');

    expect(out[2]).toHaveLength(X_BANNER.split('\n')[2].length);
    expect(out[2].startsWith('|')).toBe(true);
    expect(out[2].endsWith('|')).toBe(true);
    expect(out[3].startsWith('|')).toBe(true);
    expect(out[3].endsWith('|')).toBe(true);
  });

  it('is the same as applying one line when there is only one', () => {
    expect(applyReleaseLines(X_BANNER, [{ row: 2, text: 'one' }])).toBe(applyReleaseLineText(X_BANNER, 'one', 2));
  });

  it('reads each row back', () => {
    const filled = applyReleaseLines(X_BANNER, [
      { row: 2, text: 'the name' },
      { row: 3, text: 'the tagline' },
    ]);
    expect(readReleaseLines(filled, [2, 3])).toEqual(['the name', 'the tagline']);
  });
});
