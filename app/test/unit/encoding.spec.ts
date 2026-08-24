import { describe, expect, it, vi, beforeEach } from 'vitest';
import { toAsciiSafe, assertLatin1Safe, writeLatin1 } from '../../src/main/encoding';

vi.mock('fs/promises', () => ({
  writeFile: vi.fn().mockResolvedValue(undefined),
}));

import * as fs from 'fs/promises';

describe('toAsciiSafe', () => {
  it('transliterates common Scandinavian/German characters', () => {
    expect(toAsciiSafe('Fjälldata')).toBe('Fjaelldata');
    expect(toAsciiSafe('Pågadata')).toBe('Paagadata');
    expect(toAsciiSafe('über')).toBe('ueber');
  });

  it('leaves plain ASCII untouched', () => {
    expect(toAsciiSafe('Hello World! 123')).toBe('Hello World! 123');
  });

  it('falls back to ? for unmapped non-ASCII characters', () => {
    expect(toAsciiSafe('日本語')).toBe('???');
  });
});

describe('assertLatin1Safe', () => {
  it('accepts plain ASCII text', () => {
    expect(() => assertLatin1Safe('Hello World! 123')).not.toThrow();
  });

  it('accepts full Latin-1 range characters like ä/ö/å unchanged', () => {
    expect(() => assertLatin1Safe('Fjälldata Pågadata über')).not.toThrow();
  });

  it('throws on a curly apostrophe (U+2019), naming the character and position', () => {
    expect(() => assertLatin1Safe(`it’s here`)).toThrow(/U\+2019/);
    expect(() => assertLatin1Safe(`it’s here`)).toThrow(/character 2/);
  });

  it('throws on em/en dashes and curly quotes above U+00FF', () => {
    for (const char of ['‘', '’', '“', '”', '–', '—']) {
      expect(() => assertLatin1Safe(`text ${char} text`)).toThrow();
    }
  });
});

describe('writeLatin1', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('round-trips normal Latin-1 text (including ä/ö/å) unchanged', async () => {
    await writeLatin1('/tmp/out.nfo', 'Fjälldata Pågadata åäö');
    expect(fs.writeFile).toHaveBeenCalledTimes(1);
    const [, buf] = (fs.writeFile as any).mock.calls[0];
    expect(Buffer.isBuffer(buf)).toBe(true);
    expect(buf.toString('latin1')).toBe('Fjälldata Pågadata åäö');
  });

  it('rejects a curly apostrophe instead of silently truncating it to a stray byte', async () => {
    await expect(writeLatin1('/tmp/out.nfo', `it’s corrupted`)).rejects.toThrow(/U\+2019/);
    expect(fs.writeFile).not.toHaveBeenCalled();
  });
});
