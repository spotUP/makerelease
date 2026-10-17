import { describe, it, expect } from 'vitest';
import { describeStaleDrafts, STALE_DRAFT_AGE_SECONDS } from '../../src/main/stale-drafts';

const NOW = 1787700000;

const draft = (code: string, releaseNumber: number, ageSeconds: number) => ({
  code,
  releaseNumber,
  createdAt: NOW - ageSeconds,
  entryTitle: code.toLowerCase(),
});

describe('describeStaleDrafts', () => {
  it('says nothing when there are no drafts at all', () => {
    expect(describeStaleDrafts([], NOW)).toBeUndefined();
  });

  it('ignores the build that was just made', () => {
    expect(describeStaleDrafts([draft('EVK26', 218, 60)], NOW)).toBeUndefined();
    expect(describeStaleDrafts([draft('EVK26', 218, STALE_DRAFT_AGE_SECONDS - 1)], NOW)).toBeUndefined();
  });

  it('names an abandoned build, its release number and its age', () => {
    const message = describeStaleDrafts([draft('EVK26', 217, 3 * 24 * 60 * 60)], NOW);
    expect(message).toContain('EVK26');
    expect(message).toContain('release 217');
    expect(message).toContain('3 days ago');
    expect(message).toMatch(/publish or revoke/i);
  });

  it('calls a one-day-old draft yesterday', () => {
    expect(describeStaleDrafts([draft('EVK26', 217, STALE_DRAFT_AGE_SECONDS)], NOW)).toContain('yesterday');
  });

  it('lists several abandoned builds in release-number order', () => {
    const message = describeStaleDrafts(
      [draft('EVO26', 218, 2 * 24 * 60 * 60), draft('EVK26', 217, 5 * 24 * 60 * 60)],
      NOW,
    );
    expect(message!.indexOf('EVK26')).toBeLessThan(message!.indexOf('EVO26'));
    expect(message).toMatch(/Earlier builds were/);
  });
});
