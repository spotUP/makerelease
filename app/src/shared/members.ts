import { MemberEntry } from './types';

/**
 * Splits a member's role field into its skills: "coder, musician" ->
 * ["coder", "musician"]. Empty pieces are dropped.
 */
export function memberSkills(role: string): string[] {
  return role
    .split(',')
    .map((skill) => skill.trim())
    .filter((skill) => skill !== '');
}

/**
 * One entry per member, every skill in one comma-separated role.
 *
 * The server keeps one roster row per member and refuses a nickname listed
 * twice. The /X roster was imported from its NFO, which prints names under
 * each skill heading, so it holds one row per member and skill. Merging here
 * lets those rows (and any the user types twice) publish; the server then
 * stores the merged roster, since a publish replaces it.
 *
 * Members keep the order their first row had; skills keep first-seen order
 * and are not repeated.
 */
export function mergeMemberSkills(members: readonly MemberEntry[]): MemberEntry[] {
  const skillsByNickname = new Map<string, string[]>();
  for (const member of members) {
    const nickname = member.nickname.trim();
    if (!nickname) continue;
    const skills = skillsByNickname.get(nickname) ?? [];
    for (const skill of memberSkills(member.role)) {
      if (!skills.some((known) => known.toLowerCase() === skill.toLowerCase())) skills.push(skill);
    }
    skillsByNickname.set(nickname, skills);
  }
  return [...skillsByNickname].map(([nickname, skills]) => ({ nickname, role: skills.join(', ') }));
}
