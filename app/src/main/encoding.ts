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

export async function readLatin1(filePath: string): Promise<string> {
  const buf = await fs.readFile(filePath);
  return buf.toString('latin1');
}

export async function writeLatin1(filePath: string, text: string): Promise<void> {
  await fs.writeFile(filePath, Buffer.from(text, 'latin1'));
}
