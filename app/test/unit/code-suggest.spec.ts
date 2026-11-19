import { describe, it, expect } from 'vitest';
import { suggestReleaseCode } from '../../src/shared/code-suggest';

describe('suggestReleaseCode', () => {
  it('keeps the trailing number and trims initials to fit', () => {
    expect(suggestReleaseCode('Fleisch Bitte AD 006', 5)).toBe('FB006');
  });
  it('uses all initials when they fit', () => {
    expect(suggestReleaseCode('Fleisch Bitte AD 006', 6)).toBe('FBA006');
  });
  it('pads from the first word', () => {
    expect(suggestReleaseCode('Fleisch', 5)).toBe('FLEIS');
    expect(suggestReleaseCode('mORE dOORS', 5)).toBe('MDORE');
  });
  it('gives nothing for an empty name', () => {
    expect(suggestReleaseCode('', 5)).toBe('');
  });
});
