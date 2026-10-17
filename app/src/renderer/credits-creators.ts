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

/**
 * The names credited for a job, for the /X NFO's bracketed fields.
 *
 * Coder and Design used to be typed again in their own inputs, which is the
 * Creator(s) mistake in a second place: the same fact entered twice drifts,
 * and here it drifts between the NFO's header and its own credits list. The
 * credit rows are the single place a person is named.
 *
 * Roles are matched loosely because people write them freely - "Code",
 * "Coder", "Additional code" all mean the same job.
 */
export function creditNamesForRole(credits: CreditEntry[], pattern: RegExp): string {
  const seen = new Set<string>();
  const names: string[] = [];
  for (const credit of credits) {
    if (!pattern.test(credit.role.trim())) continue;
    const name = credit.name.trim();
    if (!name || seen.has(name.toLowerCase())) continue;
    seen.add(name.toLowerCase());
    names.push(name);
  }
  return names.join(', ');
}

/** Roles that mean "wrote the code" and "drew it", as people actually type them. */
export const CODER_ROLES = /cod|program|dev/i;
export const DESIGN_ROLES = /design|graphic|art|ink|logo|ansi|ascii/i;

/**
 * The message blocking a build for missing credits, or null. Only a renderer
 * whose output carries the creator line (the history entry) needs one; a
 * group's own template may build without any.
 */
export function creditsError(credits: CreditEntry[], needsCreatorLine: boolean): string | null {
  if (!needsCreatorLine || creatorsFromCredits(credits)) return null;
  return 'Add at least one credit - the release history creator line is built from them.';
}
