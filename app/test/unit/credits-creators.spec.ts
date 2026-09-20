import { describe, it, expect } from 'vitest';
import { creatorsFromCredits } from '../../src/renderer/credits-creators';

describe('creatorsFromCredits', () => {
  it('lists the credited names in the order they were entered', () => {
    expect(
      creatorsFromCredits([
        { role: 'Code', name: 'Nori' },
        { role: 'Music', name: 'JosSs' },
        { role: 'Graphics', name: 'Tempest' },
      ]),
    ).toBe('Nori, JosSs, Tempest');
  });

  it('names somebody credited twice only once, where they first appear', () => {
    // Two "Text" rows, and the same person on code and music - the NFO
    // creator line is a list of people, not of jobs.
    expect(
      creatorsFromCredits([
        { role: 'Code', name: 'Nori' },
        { role: 'Text', name: 'Goto80' },
        { role: 'Music', name: 'Nori' },
        { role: 'Text', name: 'Chavez' },
      ]),
    ).toBe('Nori, Goto80, Chavez');
  });

  it('treats the same name in different casing as the same person', () => {
    expect(
      creatorsFromCredits([
        { role: 'Code', name: 'Spot' },
        { role: 'Graphics', name: 'spot' },
      ]),
    ).toBe('Spot');
  });

  it('keeps the name exactly as it was typed', () => {
    expect(creatorsFromCredits([{ role: 'Music', name: 'Mortimer Twang' }])).toBe('Mortimer Twang');
  });

  it('ignores a row whose name is blank or only spaces', () => {
    expect(
      creatorsFromCredits([
        { role: 'Code', name: 'Nori' },
        { role: 'Music', name: '   ' },
      ]),
    ).toBe('Nori');
  });

  it('is empty when there are no credits', () => {
    expect(creatorsFromCredits([])).toBe('');
  });
});
