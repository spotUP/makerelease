/**
 * The placeholder renderer's template language, pure and without I/O so the
 * window's preview and the build run exactly the same code.
 *
 * `{{name}}` is replaced by its value as is. `{{name   }}` (spaces before the
 * closing braces) is a fixed-width field: the value is padded to the width of
 * the whole marker, braces included, so ASCII art around it keeps its columns;
 * a longer value is an error rather than a broken frame. A block (a list of
 * lines) must stand alone on its line, framed only by whitespace or drawing
 * characters, and repeats that frame once per value line.
 */
export const SCALAR_NAMES = [
  'group', 'url', 'code', 'filename', 'format', 'release_name',
  'platform', 'date', 'release_number', 'description', 'creators',
] as const;
export const BLOCK_NAMES = ['notes', 'news', 'members', 'credits'] as const;

export type ScalarName = (typeof SCALAR_NAMES)[number];
export type BlockName = (typeof BLOCK_NAMES)[number];

export interface TemplateValues {
  scalars: Record<ScalarName, string>;
  blocks: Record<BlockName, string[]>;
}

export class TemplateError extends Error {
  /** 1-based line number in the template. */
  readonly line: number;
  constructor(line: number, message: string) {
    super(message);
    this.line = line;
  }
}

const MARKER = /\{\{([a-z_]+)( *)\}\}/g;

function isScalar(name: string): name is ScalarName {
  return (SCALAR_NAMES as readonly string[]).includes(name);
}

function isBlock(name: string): name is BlockName {
  return (BLOCK_NAMES as readonly string[]).includes(name);
}

/** Pads a value to a fixed-width marker's width, or refuses one that does not fit. */
function fit(line: number, name: string, spaces: string, marker: string, value: string): string {
  if (spaces === '') return value;
  const width = marker.length;
  if (value.length > width) {
    throw new TemplateError(line, `value for ${name} is ${value.length} characters, field is ${width}`);
  }
  return value.padEnd(width);
}

/**
 * Text outside a block marker may be whitespace or drawing characters only:
 * a letter or digit there means the block was written inline in a sentence.
 */
const FRAME_ONLY = /^[^A-Za-z0-9]*$/;

function renderLine(text: string, line: number, values: TemplateValues): string[] {
  const markers = [...text.matchAll(MARKER)];
  for (const m of markers) {
    if (!isScalar(m[1]) && !isBlock(m[1])) {
      throw new TemplateError(line, `unknown placeholder {{${m[1]}}}`);
    }
  }
  const block = markers.find((m) => isBlock(m[1]));
  if (block) {
    const prefix = text.slice(0, block.index);
    const suffix = text.slice(block.index! + block[0].length);
    if (markers.length > 1 || !FRAME_ONLY.test(prefix) || !FRAME_ONLY.test(suffix)) {
      throw new TemplateError(line, `block placeholder {{${block[1]}}} must stand alone on its line`);
    }
    const name = block[1] as BlockName;
    return values.blocks[name].map((value) => prefix + fit(line, name, block[2], block[0], value) + suffix);
  }
  // A function replacer, never a replacement string: a value holding "$&" or
  // "$1" must come out literally.
  return [
    text.replace(MARKER, (marker: string, name: string, spaces: string) =>
      fit(line, name, spaces, marker, values.scalars[name as ScalarName]),
    ),
  ];
}

/** Renders a template whose line endings are already LF. Throws TemplateError. */
export function renderTemplate(template: string, values: TemplateValues): string {
  return template
    .split('\n')
    .flatMap((text, index) => renderLine(text, index + 1, values))
    .join('\n');
}

/**
 * Every problem renderTemplate would throw on, with its line, for the
 * window's preview. Width problems depend on the values, so they are
 * checked against the values given.
 */
export function templateProblems(template: string, values: TemplateValues): string[] {
  const problems: string[] = [];
  template.split('\n').forEach((text, index) => {
    try {
      renderLine(text, index + 1, values);
    } catch (err) {
      if (!(err instanceof TemplateError)) throw err;
      problems.push(`line ${err.line}: ${err.message}`);
    }
  });
  return problems;
}

/** CRLF and lone CR become LF, once, when a template is read. */
export function normaliseLineEndings(text: string): string {
  return text.replace(/\r\n?/g, '\n');
}
