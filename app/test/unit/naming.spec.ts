import { describe, it, expect } from 'vitest';
import { archiveFileName, releaseBaseName } from '../../src/shared/naming';

describe('release naming', () => {
  it('joins the prefix and the upper-cased code', () => {
    expect(releaseBaseName({ filePrefix: 'EX-' }, 'demo1')).toBe('EX-DEMO1');
  });

  it('allows an empty prefix', () => {
    expect(releaseBaseName({ filePrefix: '' }, 'demo1')).toBe('DEMO1');
  });

  it('names the archive after the base name and the format', () => {
    expect(archiveFileName({ filePrefix: 'UP-' }, 'mindr', 'LHA')).toBe('UP-MINDR.LHA');
    expect(archiveFileName({ filePrefix: 'UP-' }, 'mindr', 'ZIP')).toBe('UP-MINDR.ZIP');
  });
});
