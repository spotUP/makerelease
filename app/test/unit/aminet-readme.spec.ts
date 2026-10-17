import { describe, it, expect } from 'vitest';
import {
  AMINET_LINE_MAX,
  aminetFileNameProblems,
  aminetFileNames,
  aminetReadmeProblems,
  renderAminetReadme,
  type AminetReadmeInput,
} from '../../src/shared/aminet-readme';

const input: AminetReadmeInput = {
  short: 'A500 HDD demo from Chaos Constructions',
  type: 'demo/aga',
  architecture: 'm68k-amigaos',
  uploaderEmail: 'spot@uprough.net',
  uploaderName: 'Spot',
  group: { label: 'Up Rough', url: 'https://www.uprough.net' },
  entryCreators: 'Spot, Dipswitch',
  description: 'Hypocrisy, our AGA demo for Chaos Constructions 2025.',
  releaseNotesText: 'Needs 2MB chip and a hard drive.\nTested on A1200/030.',
  credits: [
    { role: 'Code', name: 'Spot' },
    { role: 'Music', name: 'Dipswitch' },
    { role: '', name: '' },
  ],
};

describe('renderAminetReadme', () => {
  it('puts the four required header fields first, Short: on line one, then a blank line and the body', () => {
    const lines = renderAminetReadme(input).split('\n');
    expect(lines[0]).toBe('Short:        A500 HDD demo from Chaos Constructions');
    expect(lines[1]).toBe('Uploader:     spot@uprough.net (Spot)');
    expect(lines[2]).toBe('Author:       Up Rough (Spot, Dipswitch)');
    expect(lines[3]).toBe('Type:         demo/aga');
    expect(lines[4]).toBe('Architecture: m68k-amigaos');
    expect(lines[5]).toBe('Distribution: Aminet');
    expect(lines[6]).toBe('');
    expect(lines[7]).toBe('Hypocrisy, our AGA demo for Chaos Constructions 2025.');
  });

  it('keeps the release notes line breaks, lists the non-empty credits and signs off with the group label and URL', () => {
    const text = renderAminetReadme(input);
    expect(text).toContain('Needs 2MB chip and a hard drive.\nTested on A1200/030.');
    expect(text).toContain('Credits:\n  Code: Spot\n  Music: Dipswitch\n');
    expect(text).not.toContain('  : ');
    expect(text.trimEnd().split('\n').pop()).toBe('Released by Up Rough - https://www.uprough.net');
    expect(text.endsWith('\n')).toBe(true);
  });

  it('wraps a long description and a long notes line at 78 columns, and uses LF only', () => {
    const text = renderAminetReadme({
      ...input,
      description: 'word '.repeat(60).trim(),
      releaseNotesText: 'note '.repeat(40).trim(),
    });
    expect(text).not.toContain('\r');
    for (const line of text.split('\n')) expect(line.length).toBeLessThanOrEqual(AMINET_LINE_MAX);
    expect(aminetReadmeProblems(text)).toEqual([]);
  });

  it('labels the author with the group alone when nobody is named', () => {
    const text = renderAminetReadme({
      ...input,
      entryCreators: '',
      group: { label: 'Example Group', url: 'https://example.net' },
    });
    expect(text).toContain('Author:       Example Group\n');
    expect(text.trimEnd().split('\n').pop()).toBe('Released by Example Group - https://example.net');
  });
});

describe('aminetReadmeProblems', () => {
  it('accepts the rendered readme', () => {
    expect(aminetReadmeProblems(renderAminetReadme(input))).toEqual([]);
  });

  it('names every missing or over-long header field', () => {
    const problems = aminetReadmeProblems(
      renderAminetReadme({
        ...input,
        short: 'x'.repeat(41),
        type: 'demo',
        architecture: '',
        uploaderEmail: 'nobody',
      }),
    );
    expect(problems).toEqual([
      'Short: is 41 characters; Aminet allows 40',
      'Uploader: needs an email address and a name, as "you@example.com (Your Name)"',
      'Type: must be an Aminet directory such as "demo/aga"',
      'Architecture: is empty',
    ]);
  });

  it('rejects an empty Short, a readme not starting with Short:, CR line ends and a line over 78 columns', () => {
    expect(aminetReadmeProblems(renderAminetReadme({ ...input, short: '' }))).toEqual(['Short: is empty']);
    expect(aminetReadmeProblems('Uploader: a@b.c (A)\nShort: x\n')).toContain('the first line must be "Short:"');
    expect(aminetReadmeProblems(renderAminetReadme(input).replace(/\n/g, '\r\n'))).toContain(
      'the readme must use LF line ends, not CR LF',
    );
    const base = renderAminetReadme(input);
    const long = `${base}${'y'.repeat(79)}\n`;
    const lineNumber = base.split('\n').length;
    expect(aminetReadmeProblems(long)).toEqual([`line ${lineNumber} is 79 characters; Aminet allows 78`]);
  });
});

describe('aminetFileNames', () => {
  it('lowercases the archive name and pairs it with a .readme of the same stem', () => {
    expect(aminetFileNames('UP-HYPOC.LHA')).toEqual({ archive: 'up-hypoc.lha', readme: 'up-hypoc.readme' });
    expect(aminetFileNames('UP-X0042.ZIP')).toEqual({ archive: 'up-x0042.zip', readme: 'up-x0042.readme' });
  });

  it('flags names over 30 characters or outside the Aminet alphabet', () => {
    expect(aminetFileNameProblems(aminetFileNames('UP-HYPOC.LHA'))).toEqual([]);
    const names = aminetFileNames(`${'a'.repeat(30)}.lha`);
    expect(aminetFileNameProblems(names)[0]).toBe(`"${'a'.repeat(30)}.lha" is 34 characters; Aminet allows 30`);
    expect(aminetFileNameProblems(aminetFileNames('up hypoc.lha'))).toEqual([
      '"up hypoc.lha" may only contain letters, digits, dots, underscores and hyphens',
      '"up hypoc.readme" may only contain letters, digits, dots, underscores and hyphens',
    ]);
  });
});
