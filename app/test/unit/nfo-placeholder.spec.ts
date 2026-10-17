import { describe, expect, it } from 'vitest';
import * as fs from 'fs';
import { placeholderProblems, placeholderValues, renderPlaceholderOutputs } from '../../src/main/nfo-placeholder';
import { EXAMPLE_DIZ_TEMPLATE, EXAMPLE_NFO_TEMPLATE } from '../../src/main/example-profile';
import { BLOCK_NAMES, SCALAR_NAMES } from '../../src/shared/placeholder-template';
import { EX_DIZ_PATH, EX_FORM, EX_NFO_PATH, EX_PROFILE } from './ex-placeholder.fixture';

const TEMPLATES = { nfo: EXAMPLE_NFO_TEMPLATE, diz: EXAMPLE_DIZ_TEMPLATE };

describe('renderPlaceholderOutputs', () => {
  it('renders the example profile byte for byte as the fixtures', () => {
    const out = renderPlaceholderOutputs(EX_PROFILE, TEMPLATES, EX_FORM);
    expect(out.nfoText).toBe(fs.readFileSync(EX_NFO_PATH, 'latin1'));
    expect(out.dizText).toBe(fs.readFileSync(EX_DIZ_PATH, 'latin1'));
    expect(out.releaseNumber).toBe(0);
  });

  it('the example NFO uses every placeholder the language knows', () => {
    // Counted from the language's own name lists, not from a search of the file.
    for (const name of [...SCALAR_NAMES, ...BLOCK_NAMES]) {
      expect(EXAMPLE_NFO_TEMPLATE, name).toContain(`{{${name}}}`);
    }
  });

  it('keeps every line of the example DIZ at its frame width', () => {
    const out = renderPlaceholderOutputs(EX_PROFILE, TEMPLATES, EX_FORM);
    const framed = out.dizText.split('\n').filter((l) => l.startsWith('|'));
    expect(framed.length).toBeGreaterThan(3);
    const widths = new Set(framed.map((l) => l.length));
    expect([...widths]).toEqual([46]);
  });

  it('ships no NFO when the profile has no release.nfo template', () => {
    const out = renderPlaceholderOutputs(EX_PROFILE, { nfo: null, diz: EXAMPLE_DIZ_TEMPLATE }, EX_FORM);
    expect(out.nfoText).toBeNull();
    expect(out.dizText).toBe(fs.readFileSync(EX_DIZ_PATH, 'latin1'));
  });

  it('adds a final newline to a template that lacks one', () => {
    const out = renderPlaceholderOutputs(EX_PROFILE, { nfo: '{{group}}', diz: '{{code}}' }, EX_FORM);
    expect(out.nfoText).toBe('Example Group\n');
    expect(out.dizText).toBe('TEST1\n');
  });

  it('names the file and line of a template problem', () => {
    expect(() => renderPlaceholderOutputs(EX_PROFILE, { nfo: 'ok\n{{bogus}}', diz: '' }, EX_FORM)).toThrow(
      'release.nfo line 2: unknown placeholder {{bogus}}',
    );
    expect(() => renderPlaceholderOutputs(EX_PROFILE, { nfo: null, diz: '{{group }}' }, EX_FORM)).toThrow(
      'file_id.diz line 1: value for group is 13 characters, field is 10',
    );
  });
});

describe('placeholderValues', () => {
  const v = placeholderValues(EX_PROFILE, EX_FORM);

  it('trims blank edges and trailing whitespace from the notes, keeping CRLF out', () => {
    expect(v.blocks.notes).toEqual(['First line of notes.', 'Second line, $& kept.']);
    expect(v.blocks.news).toEqual([]);
  });

  it('lines up member roles two columns after the longest nickname', () => {
    expect(v.blocks.members).toEqual(['coder        code', 'pixelartist  graphics']);
  });

  it('drops empty credits and formats the rest like the Aminet readme', () => {
    expect(v.blocks.credits).toEqual(['Code: coder', 'Graphics: pixelartist']);
  });

  it('upper-cases the code and names the archive with the profile prefix', () => {
    expect(v.scalars.code).toBe('TEST1');
    expect(v.scalars.filename).toBe('EX-TEST1.LHA');
  });

  it('prints an absent date or number as empty', () => {
    const bare = placeholderValues(EX_PROFILE, { ...EX_FORM, releaseDate: undefined, releaseNumberText: undefined });
    expect(bare.scalars.date).toBe('');
    expect(bare.scalars.release_number).toBe('');
  });
});

describe('placeholderProblems', () => {
  it('lists problems of both files, each with its file and line', () => {
    expect(placeholderProblems(EX_PROFILE, { nfo: '{{bogus}}', diz: 'x\n{{group }}' }, EX_FORM)).toEqual([
      'release.nfo line 1: unknown placeholder {{bogus}}',
      'file_id.diz line 2: value for group is 13 characters, field is 10',
    ]);
  });

  it('skips a missing release.nfo', () => {
    expect(placeholderProblems(EX_PROFILE, { nfo: null, diz: '{{code}}' }, EX_FORM)).toEqual([]);
  });
});
