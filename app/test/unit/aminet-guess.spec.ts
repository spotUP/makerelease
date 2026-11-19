import { describe, expect, it } from 'vitest';
import { AMINET_TYPES } from '../../src/shared/aminet-types';
import { guessAminetType } from '../../src/shared/aminet-guess';

const CATEGORIES = ['Demo', 'Intro', 'Musicdisk', 'Music', 'Graphics', 'ASCII', 'Tool', 'Door', 'Video', 'Science', 'Other'];

describe('guessAminetType', () => {
  it('splits Demo by platform', () => {
    expect(guessAminetType('Demo', 'A1200')).toBe('demo/aga');
    expect(guessAminetType('demo', 'A500')).toBe('demo/ecs');
    expect(guessAminetType('Demo', 'Console')).toBe('demo/misc');
    expect(guessAminetType('Demo', '')).toBe('demo/misc');
  });
  it('maps the other categories', () => {
    expect(guessAminetType('Intro', '')).toBe('demo/intro');
    expect(guessAminetType('Musicdisk', '')).toBe('mods/sets');
    expect(guessAminetType('Music', '')).toBe('mods/misc');
    expect(guessAminetType('Graphics', '')).toBe('pix/art');
    expect(guessAminetType('Tool', '')).toBe('util/misc');
    expect(guessAminetType('Door', '')).toBe('comm/bbs');
    expect(guessAminetType('Video', '')).toBe('pix/anim');
  });
  it('returns empty for no sensible guess', () => {
    for (const c of ['ASCII', 'Science', 'Other', 'Bogus', '']) expect(guessAminetType(c, 'A500')).toBe('');
  });
  it('only returns real Aminet directories', () => {
    const real = new Set(AMINET_TYPES.map((t) => t.type));
    for (const c of CATEGORIES) for (const p of ['', 'A500', 'A1200', 'Console', 'x']) {
      const g = guessAminetType(c, p);
      if (g) expect(real.has(g)).toBe(true);
    }
  });
});
