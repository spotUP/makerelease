import { describe, it, expect } from 'vitest';
import {
  CODER_ROLES,
  DESIGN_ROLES,
  creatorsFromCredits,
  creditNamesForRole,
  creditsError,
} from '../../src/renderer/credits-creators';

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

describe('creditNamesForRole', () => {
  const credits = [
    { role: 'Code', name: 'zALO' },
    { role: 'Graphics', name: 'diNO' },
    { role: 'Music', name: 'gOTO80' },
    { role: 'Additional code', name: 'sPOT' },
    { role: 'Logo', name: 'mORTiMER tWANG' },
  ];

  it('picks the people credited for the job, in credit order', () => {
    expect(creditNamesForRole(credits, CODER_ROLES)).toBe('zALO, sPOT');
    expect(creditNamesForRole(credits, DESIGN_ROLES)).toBe('diNO, mORTiMER tWANG');
  });

  it('matches a role however it was written', () => {
    for (const role of ['Code', 'Coder', 'CODE', 'Additional code', 'Programming', 'Dev']) {
      expect(creditNamesForRole([{ role, name: 'zALO' }], CODER_ROLES)).toBe('zALO');
    }
  });

  it('names somebody once even when credited twice for the same job', () => {
    expect(
      creditNamesForRole(
        [
          { role: 'Code', name: 'zALO' },
          { role: 'Coder', name: 'zALO' },
        ],
        CODER_ROLES,
      ),
    ).toBe('zALO');
  });

  it('is empty when nobody is credited for it, rather than guessing', () => {
    expect(creditNamesForRole([{ role: 'Music', name: 'gOTO80' }], CODER_ROLES)).toBe('');
  });
});

describe('creditsError', () => {
  const empty: Parameters<typeof creditsError>[0] = [];
  it('requires a credit where the output carries the creator line', () => {
    expect(creditsError(empty, true)).toMatch(/Add at least one credit/);
    expect(creditsError([{ role: 'Code', name: 'A' }], true)).toBeNull();
  });
  it('lets a placeholder group build without credits', () => {
    expect(creditsError(empty, false)).toBeNull();
  });
});
