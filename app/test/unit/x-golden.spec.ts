import { describe, it, expect } from 'vitest';
import * as fs from 'fs';
import * as path from 'path';
import { renderXInnovationsNfo, listArtFromBanner } from '../../src/main/nfo-x-innovations';

const fixturePath = path.join(__dirname, '../fixtures/UP-MD15.NFO');
const fixture = fs.readFileSync(fixturePath, 'latin1');
const fixtureLines = fixture.split('\n');

/** The notes exactly as the real NFO carries them, read back out of it. */
function notesFromFixture(): string {
  return fixtureLines
    .slice(38, 87)
    .map((line) => line.slice(10, 68).replace(/\s+$/, ''))
    .join('\n');
}

/**
 * Each entry's art as the file actually carries it.
 *
 * Deliberately NOT re-rendered from a banner: the seven were drawn by hand
 * over years and do not agree with each other - oo7 is centred one column
 * left of oo6, and oo1 is in title case from before the group settled on its
 * lower-case lettering. No rule reproduces all seven, and inventing one that
 * "fixes" them would silently redraw history. A real build takes each
 * entry's art from that release's stored banner, which is what this passes
 * in; listArtFromBanner's own rule for NEW releases is tested separately.
 */
function entriesFromFixture(): Array<{ displayNumber: string; art: string }> {
  const entries: Array<{ displayNumber: string; art: string }> = [];
  let current: { displayNumber: string; art: string[] } | null = null;
  for (const line of fixtureLines.slice(137, 255)) {
    const field = line.length >= 74 && line.startsWith('   | %% |') ? line.slice(9, 68) : null;
    if (field === null) continue;
    const numbered = field.match(/^ {2}(oo\d):/);
    if (numbered) {
      current = { displayNumber: numbered[1], art: [] };
      entries.push({ displayNumber: current.displayNumber, art: '' });
      continue;
    }
    if (current && field.trim() !== '') {
      current.art.push(field.slice(7).replace(/\s+$/, ''));
      entries[entries.length - 1].art = current.art.join('\n');
    }
  }
  return entries;
}

const ENTRIES = entriesFromFixture();
const RELEASES = [
  'MORE DOORS CURSOR MENU v1.5',
  'cONFERENCE cOMMANDER v1.0',
  'mORE dOORS cURSOR mENU v1.0',
  'dD gLOBAL lAST cALLERS v1.1',
  'dD gLOBAL lAST cALLERS v1.0',
  'uP-pLAYLIST v1.0',
  'Spin The Wheel 1.0 by Zalo',
];

describe('the /X NFO against the real UP-MD15', () => {
  const rendered = renderXInnovationsNfo({
    releaseName: 'mORE dOORS 1.5',
    releaseDate: '2026-06-20',
    coder: 'zALO/uP!..',
    system: 'aMiEXPRESS',
    design: 'diNO/uP!..',
    releaseNotes: notesFromFixture(),
    news: 'iF yOU bREAK iT, fIX iT!',
    members: [
      ...['sANDMAN', 'zALO', 'hAMLET', 'sPOT', 'zANER'].map((n) => ({ nickname: n, role: 'sysops' })),
      ...['sPOT', 'zALO'].map((n) => ({ nickname: n, role: 'coders' })),
      ...['gOTO80', 'diNO', 'diPSWiTCH', 'zANER', 'sKOPE', 'dMG', 'mORTiMER tWANG', 'oTRO', 'sIXX', 'zORRO', 'SPOT'].map(
        (n) => ({ nickname: n, role: 'artists' }),
      ),
    ],
    boards: [
      { name: 'uPTOWN', sysop: 'sPOT', hq: 'wEBHQ', status: 'SOON' },
      { name: 'diGiTAL hiGHWAY', sysop: 'zALO', hq: 'sHQ', status: 'ASK4IT' },
      { name: 'tHE yARD', sysop: 'zANER', hq: 'gHQ', status: 'tEMP-dOWN' },
      { name: 'fiRST diViSiON', sysop: 'hAMLET', hq: 'sHQ', status: '1337-ONLY' },
      { name: 'sANCTUARY', sysop: 'sANDMAN', hq: 'sHQ', status: 'ASK4ACCS' },
    ],
    releaseList: ENTRIES,
  });

  it('draws a frame of the right width on every framed line', () => {
    for (const [i, line] of rendered.split('\n').entries()) {
      if (!line.startsWith('   | %% |')) continue;
      expect(`${i}:${line.length}`).toBe(`${i}:74`);
    }
  });

  it('puts the release name, the fields and the sections where the original has them', () => {
    const lines = rendered.split('\n');
    expect(lines[25]).toBe(fixtureLines[25]);
    expect(lines[28]).toBe(fixtureLines[28]);
    expect(lines[29]).toBe(fixtureLines[29]);
    expect(lines).toContain(fixtureLines[103]);
    expect(lines).toContain(fixtureLines[120]);
    expect(lines).toContain(fixtureLines[135]);
  });

  it('reproduces the roster and the boards table line for line', () => {
    const lines = rendered.split('\n');
    for (const n of [107, 109, 111, 112, 113]) expect(lines).toContain(fixtureLines[n - 1]);
    for (const n of [123, 124, 125, 126, 127]) expect(lines).toContain(fixtureLines[n - 1]);
  });

  it('lists all seven previous releases, newest first', () => {
    for (const [i, name] of RELEASES.entries()) {
      expect(rendered).toContain(`  oo${RELEASES.length - i}:`);
      expect(rendered).toContain(name);
    }
  });

  it('differs from the original in exactly one line, the damaged one', () => {
    // The source NFO carries one 283-column line, a paste accident whose
    // tail is dashes and a stray gutter. The template repairs it, so this is
    // the one place the output deliberately does not match the file - and
    // pinning it here means any OTHER divergence fails this test.
    const lines = rendered.split('\n');
    const differing = lines
      .map((line, i) => (line === fixtureLines[i] ? null : i))
      .filter((i): i is number => i !== null);

    if (differing.length > 1) {
      for (const i of differing.slice(0, 4)) {
        // eslint-disable-next-line no-console
        console.log(`line ${i}\n  got  ${JSON.stringify(lines[i])}\n  want ${JSON.stringify(fixtureLines[i])}`);
      }
    }
    expect(differing).toEqual([35]);
    expect(fixtureLines[35]).toHaveLength(283);
    expect(lines[35]).toHaveLength(74);
    expect(lines).toHaveLength(fixtureLines.length);
  });
});
