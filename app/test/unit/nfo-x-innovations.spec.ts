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
  releaseNameInBanner,
  releaseNameLine,
  renderXInnovationsNfo,
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
    // Ten rows: what FILE_ID.DIZ allows, whatever the release itself shipped.
    expect(withTagline.split('\n')).toHaveLength(10);
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

describe('sections with nothing in them', () => {
  const minimal = {
    releaseName: 'test',
    releaseDate: '2026-09-21',
    coder: 'zalo',
    system: 'aMiEXPRESS',
    design: 'dino',
    releaseNotes: 'notes',
    news: '',
    members: [],
    boards: [],
    releaseList: [],
  };

  it('never leaves a bare line without a frame on it', () => {
    // An empty roster or board list used to put an empty string where the
    // section was, tearing a visible gap through the middle of the drawing.
    const rendered = renderXInnovationsNfo(minimal);
    for (const line of rendered.split('\n')) {
      expect(line === '' || line.length >= 21).toBe(true);
    }
  });

  it('keeps every framed line the right width even with nothing to show', () => {
    for (const line of renderXInnovationsNfo(minimal).split('\n')) {
      if (line.startsWith('   | %% |')) expect(line).toHaveLength(74);
    }
  });
});

describe('releaseNameInBanner', () => {
  it('reads the name out of the ten-row art this tool draws', () => {
    const art = [
      '|  `\\______|    |_|`\\_/\'\\___/`\\__/l_l_|_|  |',
      '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
      '|            tHE nEW oNE v1.0              |',
      "`------------------------------------------'",
    ].join('\n');
    expect(releaseNameInBanner(art)).toBe('tHE nEW oNE v1.0');
  });

  it('reads it out of the taller art the older releases shipped', () => {
    // up-md15 ships twelve rows with a tagline; up-stw10 ships thirteen with
    // blank rows around the name. A fixed row index reads the wrong line out
    // of both.
    const twelveRow = [
      '|                                          |',
      '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
      '|        mORE dOORS cURSOR mENU 1.5        |',
      '|      uPDATES 4 A bETTER eXPERIENCE!      |',
      "`------------------------------------------'",
    ].join('\n');
    const thirteenRow = [
      '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
      '|                                          |',
      '|        Spin The Wheel 1.0 by zALO        |',
      '|                                          |',
      "`------------------------------------------'",
    ].join('\n');

    expect(releaseNameInBanner(twelveRow)).toBe('mORE dOORS cURSOR mENU 1.5');
    expect(releaseNameInBanner(thirteenRow)).toBe('Spin The Wheel 1.0 by zALO');
  });
});

describe('replacement patterns in user text', () => {
  // String.prototype.replace reads $&, $1 and $$ in a string replacement as
  // patterns: "$&" would print the marker itself. Every value must land as
  // typed.
  it('prints $& $1 $$ literally in notes, news and fields', () => {
    const nfo = renderXInnovationsNfo({
      releaseName: 'Name $& here',
      releaseDate: '$$',
      coder: '$&',
      system: '$1',
      design: "$'",
      releaseNotes: 'costs $& $1 $$ nothing',
      news: 'news $& too',
      members: [],
      boards: [],
      releaseList: [],
    });
    expect(nfo).toContain('costs $& $1 $$ nothing');
    expect(nfo).toContain('news $& too');
    expect(nfo).not.toContain('{{');
    expect(nfo).toContain('[$&        ]');
    expect(nfo).toContain('[$$        ]');
  });
});
