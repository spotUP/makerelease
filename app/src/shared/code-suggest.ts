/**
 * A release code guessed from the release name: the initials of its words,
 * then its trailing number ("Fleisch Bitte AD 006", 5 -> "FB006"). The number
 * always survives; initials are cut to fit, and a short result is padded with
 * further letters of the first word.
 */
export function suggestReleaseCode(name: string, length: number): string {
  const upper = name.toUpperCase();
  const digits = (upper.match(/(\d+)\s*$/)?.[1] ?? '').slice(-length);
  const words = upper.replace(/\d+\s*$/, '').match(/[A-Z]+/g) ?? [];
  const room = length - digits.length;
  let letters = words.map((w) => w[0]).join('').slice(0, room);
  const first = words[0] ?? '';
  if (letters.length < room) letters += first.slice(1, 1 + room - letters.length);
  return letters + digits;
}
