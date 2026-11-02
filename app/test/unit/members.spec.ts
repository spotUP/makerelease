import { describe, it, expect } from 'vitest';
import { memberSkills, mergeMemberSkills } from '../../src/shared/members';

describe('mergeMemberSkills', () => {
  it('turns one row per skill into one row per member', () => {
    expect(
      mergeMemberSkills([
        { nickname: 'sPOT', role: 'sysops' },
        { nickname: 'zALO', role: 'coders' },
        { nickname: 'sPOT', role: 'coders' },
      ]),
    ).toEqual([
      { nickname: 'sPOT', role: 'sysops, coders' },
      { nickname: 'zALO', role: 'coders' },
    ]);
  });

  it('does not repeat a skill a member already has, whatever its case', () => {
    expect(
      mergeMemberSkills([
        { nickname: 'sPOT', role: 'coder, Musician' },
        { nickname: ' sPOT ', role: 'musician' },
      ]),
    ).toEqual([{ nickname: 'sPOT', role: 'coder, Musician' }]);
  });

  it('drops rows with no nickname', () => {
    expect(mergeMemberSkills([{ nickname: '  ', role: 'coder' }])).toEqual([]);
  });
});

describe('memberSkills', () => {
  it('splits on commas and drops empty pieces', () => {
    expect(memberSkills(' coder ,, musician ')).toEqual(['coder', 'musician']);
  });
});
