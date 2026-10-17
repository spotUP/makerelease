/**
 * Greedy word wrap. The first line may have a different width from the
 * rest, which is what an indented label line followed by continuation lines
 * needs (the NFO history block) - a plain paragraph passes the same width
 * twice. A word longer than the width stands alone on its line rather than
 * being split.
 */
export function wrapWords(text: string, firstWidth: number, contWidth: number): string[] {
  const words = text.split(/\s+/).filter(Boolean);
  const lines: string[] = [];
  let current = '';
  let width = firstWidth;
  for (const word of words) {
    const candidate = current ? `${current} ${word}` : word;
    if (candidate.length > width && current) {
      lines.push(current);
      current = word;
      width = contWidth;
    } else {
      current = candidate;
    }
  }
  if (current) lines.push(current);
  return lines.length ? lines : [''];
}
