import { DraftEntry } from './releases-client';

/**
 * A build that is never published and never revoked keeps its release number
 * and its code forever, while staying invisible in the catalogue and on the
 * site. That is not a hypothetical: an abandoned Evoke 2026 build held 217
 * for a day while the public catalogue appeared to end at 216, and the next
 * build quietly started from 218.
 *
 * Young drafts are ignored on purpose - the build you just made is a draft,
 * and warning about it would be noise. A draft that has survived a day is
 * one somebody walked away from.
 */
export const STALE_DRAFT_AGE_SECONDS = 24 * 60 * 60;

function describeAge(seconds: number): string {
  const days = Math.floor(seconds / 86400);
  if (days >= 2) return `${days} days ago`;
  return 'yesterday';
}

export function describeStaleDrafts(drafts: DraftEntry[], nowSeconds: number): string | undefined {
  const stale = drafts
    .filter((draft) => nowSeconds - draft.createdAt >= STALE_DRAFT_AGE_SECONDS)
    .sort((a, b) => a.releaseNumber - b.releaseNumber);
  if (stale.length === 0) return undefined;

  const listed = stale
    .map((draft) => `${draft.code} (release ${draft.releaseNumber}, built ${describeAge(nowSeconds - draft.createdAt)})`)
    .join(', ');
  const subject = stale.length === 1 ? 'An earlier build was' : 'Earlier builds were';
  const numbers = stale.length === 1 ? 'its release number is' : 'their release numbers are';
  return (
    `${subject} never published and never revoked: ${listed}. Nobody can see ` +
    `${stale.length === 1 ? 'it' : 'them'}, but ${numbers} spent. Publish or revoke to get ` +
    `${stale.length === 1 ? 'the number' : 'the numbers'} back.`
  );
}
