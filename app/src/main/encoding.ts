import * as fs from 'fs/promises';

const ASCII_TRANSLITERATIONS: Record<string, string> = {
  ä: 'ae', ö: 'oe', å: 'aa', ü: 'ue', é: 'e', è: 'e', ê: 'e',
  Ä: 'Ae', Ö: 'Oe', Å: 'Aa', Ü: 'Ue', É: 'E',
  ñ: 'n', ç: 'c', ø: 'oe', Ø: 'Oe', æ: 'ae', Æ: 'Ae',
};

export function toAsciiSafe(input: string): string {
  let out = '';
  for (const char of input) {
    if (char.charCodeAt(0) <= 0x7f) {
      out += char;
    } else {
      out += ASCII_TRANSLITERATIONS[char] ?? '?';
    }
  }
  return out;
}

/**
 * Throws if `text` contains any character outside the Latin-1 range
 * (codepoints 0-255). `Buffer.from(text, 'latin1')` truncates any higher
 * codepoint to its low byte instead of erroring — e.g. a curly apostrophe
 * (U+2019, common from macOS auto-correct/copy-paste) silently becomes the
 * meaningless byte 0x19. Real historical NFOs legitimately use the full
 * Latin-1 range (e.g. "Fjälldata"), so this only rejects codepoints above
 * 0xFF — it must not transliterate or otherwise touch anything in range.
 */
export function assertLatin1Safe(text: string): void {
  let position = 0;
  for (const char of text) {
    const codePoint = char.codePointAt(0) as number;
    if (codePoint > 0xff) {
      const hex = codePoint.toString(16).toUpperCase().padStart(4, '0');
      throw new Error(
        `Text contains "${char}" (U+${hex}) at character ${position}, which is outside the Latin-1 range ` +
          '(0-255) and would be silently corrupted if written. Replace it with a plain Latin-1 character ' +
          "(e.g. a straight quote ' instead of a curly one) and try again.",
      );
    }
    position += char.length;
  }
}

export async function writeLatin1(filePath: string, text: string): Promise<void> {
  assertLatin1Safe(text);
  await fs.writeFile(filePath, Buffer.from(text, 'latin1'));
}
