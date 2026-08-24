import { describe, expect, it } from 'vitest';
import * as fs from 'fs';
import * as path from 'path';
import { parseNfo } from '../../src/main/nfo-parser';

const fixturePath = path.join(__dirname, '..', 'fixtures', 'UP-FB006.NFO');
const fixtureText = fs.readFileSync(fixturePath, 'latin1');

describe('parseNfo', () => {
  const parsed = parseNfo(fixtureText, fixturePath);

  it('parses all 36 members', () => {
    expect(parsed.members.length).toBe(36);
  });

  it('spot-checks known member entries', () => {
    expect(parsed.members.find((m) => m.nickname === 'the soul collector')).toEqual({
      nickname: 'the soul collector',
      role: 'ink',
    });
    expect(parsed.members.find((m) => m.nickname === 'spot')).toEqual({
      nickname: 'spot',
      role: 'leader, ink, stereofonik development',
    });
  });

  it('parses all 208 history entries', () => {
    expect(parsed.history.length).toBe(208);
  });

  it('computes maxReleaseNumber as 208, ignoring non-numeric legacy entries', () => {
    expect(parsed.maxReleaseNumber).toBe(208);
    const nullEntries = parsed.history.filter((h) => h.releaseNumberValue === null);
    expect(nullEntries.length).toBeGreaterThan(0);
  });

  it('extracts the current release notes title line', () => {
    expect(parsed.currentReleaseNotesTitleLine).toContain("'FLEISCH BITTE AD 006'");
    expect(parsed.currentReleaseNotesTitleLine).toContain('(A1200)');
  });

  it('extracts release notes and news free text', () => {
    expect(parsed.currentReleaseNotesText).toContain('FINAL VERSION');
    expect(parsed.currentNewsText).toContain('Pågadata');
  });

  it('carries a raw block for the wrapped release 204 creators line', () => {
    const entry = parsed.history.find((h) => h.releaseNumberRaw === '204');
    expect(entry).toBeDefined();
    expect(entry!.rawBlock.some((l) => l.includes('mortimer twang, sk!n'))).toBe(true);
    expect(entry!.creators).toContain('mortimer twang, sk!n');
  });
});
