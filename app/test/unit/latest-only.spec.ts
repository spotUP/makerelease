import { describe, it, expect } from 'vitest';
import { LatestOnly } from '../../src/renderer/latest-only';

describe('LatestOnly', () => {
  it('lets only the most recently begun run apply its result', () => {
    const switches = new LatestOnly();
    const first = switches.begin();
    const second = switches.begin();
    expect(first()).toBe(false);
    expect(second()).toBe(true);
  });
});
