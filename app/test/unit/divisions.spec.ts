import { describe, it, expect } from 'vitest';
import { DIVISIONS, DEFAULT_DIVISION, divisionOf, isDivisionId } from '../../src/shared/divisions';
import { findReleaseLineSlot, readReleaseLineText } from '../../src/shared/diz-release-line';

describe('divisions', () => {
  it('defaults to Up Rough', () => {
    expect(DEFAULT_DIVISION).toBe('uprough');
    expect(divisionOf(DEFAULT_DIVISION).label).toBe('Up Rough');
  });

  it('rejects anything that is not one of the two', () => {
    expect(isDivisionId('uprough')).toBe(true);
    expect(isDivisionId('xinnovations')).toBe(true);
    expect(isDivisionId('demoscene')).toBe(false);
    expect(isDivisionId(undefined)).toBe(false);
  });

  it('gives every division exactly one editable line', () => {
    expect(DIVISIONS.uprough.editableDizRows).toHaveLength(1);
    expect(DIVISIONS.xinnovations.editableDizRows.map((r) => r.label)).toEqual(['Release line']);
  });

  it('keeps every banner inside what FILE_ID.DIZ allows: ten rows of 45 columns', () => {
    for (const division of Object.values(DIVISIONS)) {
      const lines = division.defaultDizBanner.split('\n');
      expect(lines.length).toBeLessThanOrEqual(10);
      for (const line of lines) expect(line.length).toBeLessThanOrEqual(45);
    }
  });

  it('points every editable row at a line that is actually blank in the art', () => {
    // A row pointing at the logo would let a release overwrite the drawing.
    for (const division of Object.values(DIVISIONS)) {
      for (const spec of division.editableDizRows) {
        expect(readReleaseLineText(division.defaultDizBanner, spec.row)).toBe('');
      }
    }
  });

  it('keeps the division name fixed, above the editable rows', () => {
    const lines = DIVISIONS.xinnovations.defaultDizBanner.split('\n');
    expect(lines[7]).toContain('uP rOUGH /X iNNOVATiONS pRESENTS:');
    expect(DIVISIONS.xinnovations.editableDizRows.every((r) => r.row > 7)).toBe(true);
    // The /X art carries its own name, never the plain Up Rough one.
    expect(DIVISIONS.xinnovations.defaultDizBanner).not.toContain('Up Rough Presents:');
    expect(DIVISIONS.uprough.defaultDizBanner).toContain('Up Rough Presents:');
  });

  it('gives both divisions the same 45-column art width', () => {
    // The /X NFO embeds each release's banner in a fixed-width frame, so the
    // two divisions' art has to measure the same.
    const up = findReleaseLineSlot(DIVISIONS.uprough.defaultDizBanner, DIVISIONS.uprough.editableDizRows[0].row);
    const x = findReleaseLineSlot(
      DIVISIONS.xinnovations.defaultDizBanner,
      DIVISIONS.xinnovations.editableDizRows[0].row,
    );
    expect(x.width).toBe(up.width);
  });

  it('sends only /X releases to the door repository', () => {
    expect(DIVISIONS.xinnovations.submitsToDoorRepo).toBe(true);
    expect(DIVISIONS.uprough.submitsToDoorRepo).toBe(false);
  });
});
