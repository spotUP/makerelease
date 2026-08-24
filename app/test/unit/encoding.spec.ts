import { describe, expect, it } from 'vitest';
import { toAsciiSafe } from '../../src/main/encoding';

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
