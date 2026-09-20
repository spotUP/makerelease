import { describe, it, expect } from 'vitest';
import * as fs from 'fs';
import * as path from 'path';
import {
  ENTRY_RULE,
  FIELD_WIDTH,
  boardsBlock,
  centerInField,
  frameLine,
  listArtFromBanner,
  releaseListBlock,
  releaseNameLine,
  rosterBlock,
  spacedCaps,
  wrapNotes,
} from '../../src/main/nfo-x-innovations';

const fixture = fs.readFileSync(path.join(__dirname, '../fixtures/UP-MD15.NFO'), 'latin1').split('\n');

describe('the /X frame', () => {
  it('draws a content line the same width the real NFO does', () => {
    // Line 33 of the real file (1-based 33) is a plain content line.
    expect(frameLine('').length).toBe(74);
    expect(frameLine('')).toBe(fixture[36]);
  });

  it('draws the entry rule exactly as the original', () => {
    expect(ENTRY_RULE).toBe(fixture[137]);
  });

  it('never lets text push the right gutter out of place', () => {
    expect(frameLine('x'.repeat(200))).toHaveLength(74);
    expect(frameLine('x'.repeat(200)).endsWith('| %% |')).toBe(true);
  });

  it('centres the way the original does', () => {
    expect(centerInField('ab')).toHaveLength(FIELD_WIDTH);
    // The release-name line of the real file, rebuilt.
    expect(releaseNameLine('mORE dOORS 1.5')).toBe(fixture[25]);
  });
});

describe('wrapNotes', () => {
  it('leaves hand-laid-out lines exactly as typed', () => {
    const notes = ['Description', '-----------', 'Second-level cursor menu for AmiExpress BBS systems.'].join('\n');
    expect(wrapNotes(notes)).toEqual([
      'Description',
      '-----------',
      'Second-level cursor menu for AmiExpress BBS systems.',
    ]);
  });

  it('breaks a line too wide for the frame, keeping its indent', () => {
    const wrapped = wrapNotes(`    ${'word '.repeat(30).trim()}`);
    expect(wrapped.length).toBeGreaterThan(1);
    // One column goes to the space that sets notes in from the gutter.
    for (const line of wrapped) expect(line.length).toBeLessThanOrEqual(FIELD_WIDTH - 1);
    expect(wrapped[1].startsWith('    ')).toBe(true);
  });
});

describe('rosterBlock', () => {
  it('prints the groups the way the real NFO does', () => {
    const block = rosterBlock([
      { nickname: 'sANDMAN', role: 'sysops' },
      { nickname: 'zALO', role: 'sysops' },
      { nickname: 'hAMLET', role: 'sysops' },
      { nickname: 'sPOT', role: 'sysops' },
      { nickname: 'zANER', role: 'sysops' },
      { nickname: 'sPOT', role: 'coders' },
      { nickname: 'zALO', role: 'coders' },
      { nickname: 'gOTO80', role: 'artists' },
    ]);
    expect(block[0]).toBe(fixture[106]);
    expect(block.every((line) => line.length === 74)).toBe(true);
  });

  it('writes a role the way the NFO spaces its headings', () => {
    expect(spacedCaps('sysops')).toBe('s Y S O P S');
    // Their lettering keeps every i lower case, as the real file prints it.
    expect(spacedCaps('artists')).toBe('a R T i S T S');
  });

  it('continues a long group on an aligned line rather than breaking the frame', () => {
    const many = Array.from({ length: 12 }, (_, i) => ({ nickname: `nickname${i}`, role: 'artists' }));
    const block = rosterBlock(many);
    expect(block.length).toBeGreaterThan(1);
    expect(block.every((line) => line.length === 74)).toBe(true);
  });
});

describe('boardsBlock', () => {
  it('keeps every row inside the frame', () => {
    const block = boardsBlock([
      { name: 'uPTOWN', sysop: 'sPOT', hq: 'wEBHQ', status: 'SOON' },
      { name: 'diGiTAL hiGHWAY', sysop: 'zALO', hq: 'sHQ', status: 'ASK4IT' },
    ]);
    expect(block.every((line) => line.length === 74)).toBe(true);
    expect(block[0]).toContain('uPTOWN');
    expect(block[0]).toContain('SOON');
  });
});

describe('releaseListBlock', () => {
  const art = ['|                                          |', '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |'].join(
    '\n',
  );

  it('numbers each entry and frames its art', () => {
    const block = releaseListBlock([{ displayNumber: 'oo7', art }]);
    expect(block[0]).toBe(ENTRY_RULE);
    expect(block[2]).toBe(fixture[139]);
    expect(block.every((line) => line.length === 74 || line === ENTRY_RULE)).toBe(true);
  });

  it('indents the art the way the real entries are indented', () => {
    const block = releaseListBlock([{ displayNumber: 'oo7', art: '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |' }]);
    expect(block[3]).toBe(fixture[148]);
  });
});

describe('listArtFromBanner', () => {
  it('keeps the release name and drops the tagline', () => {
    const shipped = fs.readFileSync(path.join(__dirname, '../fixtures/UP-MD15-FILE_ID.DIZ'), 'latin1');
    const art = listArtFromBanner(shipped).split('\n');

    expect(shipped).toContain('uPDATES 4 A bETTER eXPERIENCE!');
    expect(art.join('\n')).toContain('mORE dOORS cURSOR mENU 1.5');
    expect(art.join('\n')).not.toContain('uPDATES 4 A bETTER eXPERIENCE!');
  });

  it('gives every entry the same shape, whatever the release shipped', () => {
    const withTagline = listArtFromBanner(
      fs.readFileSync(path.join(__dirname, '../fixtures/UP-MD15-FILE_ID.DIZ'), 'latin1'),
    );
    const nameOnly = listArtFromBanner(withTagline);
    expect(nameOnly).toBe(withTagline);
    // Eleven rows, not twelve: the tagline row is dropped, not blanked.
    expect(withTagline.split('\n')).toHaveLength(11);
  });
});

describe('boards against the original', () => {
  it('matches the real table row for row', () => {
    const block = boardsBlock([
      { name: 'uPTOWN', sysop: 'sPOT', hq: 'wEBHQ', status: 'SOON' },
      { name: 'diGiTAL hiGHWAY', sysop: 'zALO', hq: 'sHQ', status: 'ASK4IT' },
      { name: 'tHE yARD', sysop: 'zANER', hq: 'gHQ', status: 'tEMP-dOWN' },
      { name: 'fiRST diViSiON', sysop: 'hAMLET', hq: 'sHQ', status: '1337-ONLY' },
      { name: 'sANCTUARY', sysop: 'sANDMAN', hq: 'sHQ', status: 'ASK4ACCS' },
    ]);
    expect(block).toEqual(fixture.slice(122, 127));
  });
});
