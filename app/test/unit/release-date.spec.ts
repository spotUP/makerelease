import { describe, it, expect } from 'vitest';
import { todayIso } from '../../src/shared/release-date';

describe('todayIso', () => {
  it('writes the date the way the NFO slot is drawn for', () => {
    expect(todayIso(new Date(2026, 5, 20, 12, 0))).toBe('2026-06-20');
  });

  it('pads single-digit months and days', () => {
    expect(todayIso(new Date(2026, 0, 5, 12, 0))).toBe('2026-01-05');
  });

  it('uses the local day, not UTC - a release built late is dated that day', () => {
    // 23:30 local on the 20th is already the 21st in UTC.
    const lateEvening = new Date(2026, 5, 20, 23, 30);
    expect(todayIso(lateEvening)).toBe('2026-06-20');
  });
});
