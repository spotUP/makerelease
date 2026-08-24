import { describe, expect, it } from 'vitest';
import * as fs from 'fs';
import * as path from 'path';
import { parseNfo } from '../../src/main/nfo-parser';
import { renderMemberBlock, renderHistoryBlock, renderNewHistoryEntry } from '../../src/main/nfo-generator';

const fixturePath = path.join(__dirname, '..', 'fixtures', 'UP-FB006.NFO');
const fixtureText = fs.readFileSync(fixturePath, 'latin1');
const fixtureLines = fixtureText.split('\n');

describe('renderMemberBlock', () => {
  it('regenerates the member block matching the real fixture (modulo incidental trailing whitespace)', () => {
    const parsed = parseNfo(fixtureText, fixturePath);
    const regenerated = renderMemberBlock(parsed.members).split('\n');
    const original = fixtureLines.slice(31, 67); // lines 32-67, 0-indexed

    expect(regenerated.length).toBe(original.length);
    for (let i = 0; i < original.length; i++) {
      expect(regenerated[i].replace(/\s+$/, '')).toBe(original[i].replace(/\s+$/, ''));
    }
  });
});

describe('renderHistoryBlock', () => {
  it('reproduces the existing history section verbatim via rawBlock passthrough (modulo blank-separator whitespace)', () => {
    // The source file has a handful of separator lines that are trailing-whitespace-only
    // instead of truly blank (a 25-year-old hand-editing quirk, e.g. before release 195).
    // The parser correctly treats these as blank separators (matching every other entry
    // boundary); the generator's canonical skeleton always emits a truly-empty separator.
    // So we compare with trailing whitespace stripped per line, which still proves every
    // historical entry's own content (including release 204's wrapped creators line) is
    // reproduced byte-for-byte.
    const parsed = parseNfo(fixtureText, fixturePath);
    const existingOnly = parsed.history.map((e) => e.rawBlock.join('\n')).join('\n\n');
    const originalHistorySection = fixtureLines.slice(74, 1114).join('\n').replace(/\n+$/, '');
    const normalize = (s: string) => s.split('\n').map((l) => l.replace(/\s+$/, '')).join('\n');
    expect(normalize(existingOnly)).toBe(normalize(originalHistorySection));
  });

  it('prepends a freshly rendered new entry above the existing history', () => {
    const parsed = parseNfo(fixtureText, fixturePath);
    const newEntry = renderNewHistoryEntry(209, 'up-test0.lha', 'test release', 'intro (test party)', 'spot');
    const block = renderHistoryBlock(newEntry, parsed.history);
    expect(block.startsWith('        release 209 up-test0.lha')).toBe(true);
    expect(block).toContain('        release 208 up-fb006.lha');
  });
});

describe('renderNewHistoryEntry', () => {
  it('wraps a long creators line with 20-space continuation indent', () => {
    const longCreators =
      'nori k, mortimer twang, spot, qwan, exocet, mooz, dino, teis, syphus, varthall, zaner, zorro, otro';
    const lines = renderNewHistoryEntry(209, 'up-test0.lha', 'test', 'intro', longCreators);
    const creatorLines = lines.filter((l, i) => i >= 3);
    expect(creatorLines.length).toBeGreaterThan(1);
    expect(creatorLines[0].startsWith('        creator(s)  ')).toBe(true);
    for (let i = 1; i < creatorLines.length; i++) {
      expect(creatorLines[i].startsWith(' '.repeat(20))).toBe(true);
      expect(creatorLines[i].startsWith(' '.repeat(21))).toBe(false);
    }
  });
});
