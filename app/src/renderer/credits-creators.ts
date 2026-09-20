import { CreditEntry } from '../shared/types';

// Who made a release used to be typed twice: once as the free-text
// "Creator(s)" line the NFO history entry needs, and once as the role/name
// credit rows the WordPress post needs. Two inputs for one fact drift apart -
// a name fixed in one is stale in the other, and the NFO is the copy that
// ships to traders. The credit rows are now the only place it is entered and
// the NFO line is derived from them.
//
// Order is the order the credits were entered, which is meaningful (code,
// music, graphics...). A name credited twice - two "Text" rows, or someone
// who both coded and made the music - appears once, at the position of its
// first credit.
export function creatorsFromCredits(credits: CreditEntry[]): string {
  const seen = new Set<string>();
  const names: string[] = [];
  for (const credit of credits) {
    const name = credit.name.trim();
    if (!name) continue;
    const key = name.toLowerCase();
    if (seen.has(key)) continue;
    seen.add(key);
    names.push(name);
  }
  return names.join(', ');
}
