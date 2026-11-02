import { describe, it, expect } from 'vitest';
import { Prefill, releaseNameText } from '../../src/renderer/prefill';

describe('releaseNameText', () => {
  it('drops the quotes the release notes put around the name', () => {
    expect(releaseNameText("'FLEISCH BITTE AD 006'")).toBe('FLEISCH BITTE AD 006');
    expect(releaseNameText(' "mORE dOORS" ')).toBe('mORE dOORS');
  });

  it('keeps quotes that do not wrap the whole name', () => {
    expect(releaseNameText("Party Like It's 1996")).toBe("Party Like It's 1996");
  });
});

describe('Prefill', () => {
  it('follows the source while the field is untouched', () => {
    const prefill = new Prefill();
    expect(prefill.next('', 'F')).toBe('F');
    expect(prefill.next('F', 'FL')).toBe('FL');
  });

  it('replaces the starting text when that text was itself a prefill', () => {
    const prefill = new Prefill('LAST RELEASE');
    expect(prefill.next('LAST RELEASE', 'NEW')).toBe('NEW');
  });

  it('stops following once the user typed their own text', () => {
    const prefill = new Prefill();
    prefill.next('', 'NAME');
    expect(prefill.next('NAME released at Revision', 'NAME 2')).toBeUndefined();
  });

  it('follows again after the user clears the field', () => {
    const prefill = new Prefill();
    prefill.next('', 'NAME');
    expect(prefill.next('', 'NAME 2')).toBe('NAME 2');
  });
});
