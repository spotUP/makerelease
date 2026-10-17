import { describe, expect, it } from 'vitest';
import { renderMemberBlock, renderHistoryBlock, renderHistoryEntry, renderNfo, renderDiz, generateOutputs } from '../../src/main/nfo-generator';
import { MemberEntry, ReleaseHistoryEntry, ReleaseFormData } from '../../src/shared/types';

const MEMBERS: MemberEntry[] = [
  { nickname: 'the soul collector', role: 'ink' },
  { nickname: 'spot', role: 'leader, ink, stereofonik development' },
  { nickname: 'qwan', role: 'co-leader, stereofonik development' },
];

describe('renderMemberBlock', () => {
  it('pads every nickname to the widest nickname + 2, minimum 20', () => {
    const block = renderMemberBlock(MEMBERS).split('\n');
    // "the soul collector" is 18 chars, so pad width is max(20, 18+2) = 20
    expect(block[0]).toBe('        the soul collector  ink');
    expect(block[1]).toBe('        spot                leader, ink, stereofonik development');
  });

  it('uses the minimum width of 20 when every nickname is short', () => {
    const block = renderMemberBlock([{ nickname: 'ab', role: 'x' }]).split('\n');
    expect(block[0]).toBe('        ab                  x');
  });
});

describe('renderHistoryEntry', () => {
  it('renders the four-line label-padded block for a short entry', () => {
    const entry: ReleaseHistoryEntry = {
      releaseNumber: '208',
      filenames: 'up-fb006.lha',
      title: 'fleisch bitte ad 006',
      category: 'intro (fjälldata 2023)',
      creators: 'nori k, mortimer twang, spot, qwan, exocet',
    };
    const lines = renderHistoryEntry(entry);
    expect(lines[0]).toBe('        release 208 up-fb006.lha');
    expect(lines[1]).toBe('        title       fleisch bitte ad 006');
    expect(lines[2]).toBe('        category    intro (fjälldata 2023)');
    expect(lines[3]).toBe('        creator(s)  nori k, mortimer twang, spot, qwan, exocet');
  });

  it('wraps a long creators line with 20-space continuation indent', () => {
    const entry: ReleaseHistoryEntry = {
      releaseNumber: '204',
      filenames: 'ds!up-a1.dms',
      title: 'asciiexpress issue #001',
      category: 'tool',
      creators: 'k-p/hippopotamus dezign, dino, goto80, phantasm, spot, mortimer twang, sk!n',
    };
    const lines = renderHistoryEntry(entry);
    const creatorLines = lines.slice(3);
    expect(creatorLines.length).toBeGreaterThan(1);
    expect(creatorLines[0].startsWith('        creator(s)  ')).toBe(true);
    for (let i = 1; i < creatorLines.length; i++) {
      expect(creatorLines[i].startsWith(' '.repeat(20))).toBe(true);
      expect(creatorLines[i].startsWith(' '.repeat(21))).toBe(false);
    }
  });
});

describe('renderHistoryBlock', () => {
  it('renders every entry uniformly and joins with a blank line, newest first', () => {
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: '209', filenames: 'up-test0.lha', title: 'test', category: 'Demo', creators: 'spot' },
      { releaseNumber: '208', filenames: 'up-fb006.lha', title: 'fleisch bitte ad 006', category: 'intro', creators: 'nori k' },
    ];
    const block = renderHistoryBlock(history);
    expect(block.startsWith('        release 209 up-test0.lha')).toBe(true);
    expect(block).toContain('        release 208 up-fb006.lha');
    expect(block).toContain('\n\n        release 208');
  });

  it('renders a legacy non-numeric releaseNumber as an opaque string, unparsed', () => {
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: 'o99', filenames: 'up-legacy.lha', title: 'legacy import', category: 'intro', creators: 'unknown' },
    ];
    const block = renderHistoryBlock(history);
    expect(block.startsWith('        release o99 up-legacy.lha')).toBe(true);
  });
});

const UR = { filePrefix: 'UP-', label: 'Up Rough', url: 'https://uprough.net', renderer: 'uprough' } as const;

const sampleForm: ReleaseFormData = {
  profileId: 'uprough',
  code: 'TEST0',
  format: 'LHA',
  releaseName: "'TEST RELEASE'",
  platformTag: 'A1200',
  releaseNotesText: 'Some notes.',
  newsText: 'Some news.',
  dizBannerText: 'diz art',
  members: MEMBERS,
  entryTitle: 'test release',
  entryCategory: 'Demo',
  entryCreators: 'spot',
  selectedPaths: [],
};

describe('renderNfo', () => {
  it('embeds the release-notes line, banner, members, and history block', () => {
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: '217', filenames: 'up-test0.lha', title: 'test release', category: 'Demo', creators: 'spot' },
    ];
    const nfo = renderNfo(UR, history, sampleForm, 217);
    expect(nfo).toContain("Release named 'TEST RELEASE' (A1200) (UP-TEST0.LHA)");
    expect(nfo).toContain('Some notes.');
    expect(nfo).toContain('Some news.');
    expect(nfo).toContain('spot                leader, ink, stereofonik development');
    expect(nfo).toContain('release 217 up-test0.lha');
  });
});

describe('renderDiz', () => {
  it('keeps leading spaces on the first line of indented ASCII art', () => {
    const diz = renderDiz({ ...sampleForm, dizBannerText: '   /\\   \n  /__\\  ' });
    expect(diz).toBe('   /\\\n  /__\\\n');
  });

  it('still drops grid padding: trailing spaces per line and blank leading/trailing rows', () => {
    const diz = renderDiz({ ...sampleForm, dizBannerText: '\n  art   \n     \n\n' });
    expect(diz).toBe('  art\n');
  });
});

describe('generateOutputs', () => {
  it('uses the passed-in releaseNumber directly, does not recompute it', () => {
    // In real usage (Task 14), the caller passes history that already includes
    // the just-inserted release as history[0] (per POST /releases' response) —
    // generateOutputs no longer synthesizes an entry from releaseNumber itself.
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: '42', filenames: 'up-test0.lha', title: 'test release', category: 'Demo', creators: 'spot' },
    ];
    const result = generateOutputs(UR, history, 42, sampleForm);
    expect(result.releaseNumber).toBe(42);
    expect(result.nfoText).toContain('release 42');
  });

  it('does not synthesize a history entry — an empty history produces an empty history block', () => {
    const result = generateOutputs(UR, [], 42, sampleForm);
    expect(result.releaseNumber).toBe(42);
    expect(result.nfoText).not.toContain('release 42');
    // The history section should still have its heading, just no entry beneath it.
    const historySectionIndex = result.nfoText.indexOf('        r e l e a s e d   s o   f a r');
    expect(historySectionIndex).toBeGreaterThan(-1);
  });
});
