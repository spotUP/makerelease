import { describe, expect, it } from 'vitest';
import {
  BLOCK_NAMES,
  SCALAR_NAMES,
  TemplateError,
  normaliseLineEndings,
  renderTemplate,
  templateProblems,
  type TemplateValues,
} from '../../src/shared/placeholder-template';

function values(over: Partial<TemplateValues['scalars']> = {}, blocks: Partial<TemplateValues['blocks']> = {}): TemplateValues {
  const scalars = Object.fromEntries(SCALAR_NAMES.map((n) => [n, `<${n}>`])) as TemplateValues['scalars'];
  const lists = Object.fromEntries(BLOCK_NAMES.map((n) => [n, [`${n} one`, `${n} two`]])) as TemplateValues['blocks'];
  return { scalars: { ...scalars, ...over }, blocks: { ...lists, ...blocks } };
}

function errorOf(fn: () => unknown): TemplateError {
  try {
    fn();
  } catch (err) {
    if (err instanceof TemplateError) return err;
    throw err;
  }
  throw new Error('expected a TemplateError');
}

describe('placeholder template language', () => {
  it('knows exactly 15 placeholders, 11 scalars and 4 blocks', () => {
    expect(SCALAR_NAMES).toHaveLength(11);
    expect(BLOCK_NAMES).toHaveLength(4);
    expect(new Set([...SCALAR_NAMES, ...BLOCK_NAMES]).size).toBe(15);
  });

  it.each(SCALAR_NAMES)('replaces the scalar {{%s}}', (name) => {
    expect(renderTemplate(`a {{${name}}} b`, values())).toBe(`a <${name}> b`);
  });

  it.each(BLOCK_NAMES)('expands the block {{%s}} into one line per value', (name) => {
    expect(renderTemplate(`top\n{{${name}}}\nend`, values())).toBe(`top\n${name} one\n${name} two\nend`);
  });

  it('repeats a block line frame of drawing characters around every value', () => {
    expect(renderTemplate('| {{notes}} |', values({}, { notes: ['a', 'b'] }))).toBe('| a |\n| b |');
  });

  it('removes a block line whose value list is empty', () => {
    expect(renderTemplate('x\n  {{news}}\ny', values({}, { news: [] }))).toBe('x\ny');
  });

  it('pads a fixed-width scalar to the width of the whole marker', () => {
    const out = renderTemplate('[{{code     }}]', values({ code: 'AB' }));
    expect(out).toBe(`[${'AB'.padEnd('{{code     }}'.length)}]`);
    expect(out).toHaveLength('[{{code     }}]'.length);
  });

  it('pads a fixed-width block to the marker width, keeping a frame aligned', () => {
    const out = renderTemplate('| {{members      }} |', values({}, { members: ['spot  code', 'x'] }));
    for (const line of out.split('\n')) expect(line).toHaveLength('| {{members      }} |'.length);
  });

  it('refuses a value longer than its fixed-width field, naming the line', () => {
    const err = errorOf(() => renderTemplate('ok\n{{code }}', values({ code: 'TOO-LONG-CODE' })));
    expect(err.line).toBe(2);
    expect(err.message).toBe(`value for code is 13 characters, field is ${'{{code }}'.length}`);
  });

  it('refuses an unknown placeholder, naming the line', () => {
    const err = errorOf(() => renderTemplate('a\nb\n{{bogus}}', values()));
    expect(err.line).toBe(3);
    expect(err.message).toBe('unknown placeholder {{bogus}}');
  });

  it('refuses a block written inside a sentence', () => {
    const err = errorOf(() => renderTemplate('Notes: {{notes}}', values()));
    expect(err.message).toBe('block placeholder {{notes}} must stand alone on its line');
  });

  it('refuses a block sharing its line with another placeholder', () => {
    expect(() => renderTemplate('{{notes}} {{code}}', values())).toThrow(TemplateError);
  });

  it('leaves text that only looks like a placeholder alone', () => {
    expect(renderTemplate('{{ code }} {code} {{CODE}}', values())).toBe('{{ code }} {code} {{CODE}}');
  });

  it('writes $& and $1 in a value literally', () => {
    expect(renderTemplate('{{release_name}}', values({ release_name: 'Cash $& $1 $$' }))).toBe('Cash $& $1 $$');
  });

  it('keeps the trailing newline of a template', () => {
    expect(renderTemplate('{{group}}\n', values({ group: 'G' }))).toBe('G\n');
  });

  it('lists every problem with its line instead of stopping at the first', () => {
    expect(templateProblems('{{bogus}}\nfine\nNotes: {{notes}}\n{{code }}', values({ code: 'MUCHLONGER' }))).toEqual([
      'line 1: unknown placeholder {{bogus}}',
      'line 3: block placeholder {{notes}} must stand alone on its line',
      `line 4: value for code is 10 characters, field is ${'{{code }}'.length}`,
    ]);
  });

  it('reports no problems for a template that renders', () => {
    expect(templateProblems('{{group}}\n{{notes}}', values())).toEqual([]);
  });

  it('normalises CRLF and lone CR to LF', () => {
    expect(normaliseLineEndings('a\r\nb\rc\n')).toBe('a\nb\nc\n');
  });
});
