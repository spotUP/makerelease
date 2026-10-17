import type { CreditEntry } from './types';
import { wrapWords } from './text-wrap';

/**
 * The .readme that travels with every Aminet upload, in the format the
 * Aminet team documents (wiki.aminet.net/The_Readme_file and
 * wiki.aminet.net/Uploading_instructions): a block of "Field: value" header
 * lines with Short: first, a blank line, then free text. Four fields are
 * required - Short, Uploader, Type and Architecture - and the whole file is
 * capped at 78 columns with LF line ends, which is why the renderer and the
 * check live together here and both ends of the app use them: the renderer
 * previews exactly what main will upload.
 */

/** Short: is what the Aminet index prints, so it is capped hard. */
export const AMINET_SHORT_MAX = 40;
export const AMINET_LINE_MAX = 78;
/** Including the suffix: "up-hypoc.readme" counts 15. */
export const AMINET_FILENAME_MAX = 30;

export const AMINET_ARCHITECTURES = [
  'm68k-amigaos',
  'ppc-amigaos',
  'ppc-morphos',
  'ppc-powerup',
  'ppc-warpup',
  'i386-aros',
  'i386-amithlon',
  'generic',
] as const;

/**
 * The Aminet directories a group release is most likely to land in. The
 * field accepts any directory; these only save typing.
 */
export const AMINET_COMMON_TYPES = [
  'demo/aga',
  'demo/ecs',
  'demo/intro',
  'demo/40k',
  'demo/disk',
  'demo/mag',
  'demo/misc',
  'demo/slide',
  'demo/sound',
  'demo/track',
  'mods/demo',
  'mods/chip',
  'mods/misc',
  'pix/art',
  'pix/illu',
  'game/misc',
  'util/misc',
  'comm/bbs',
] as const;

export interface AminetReadmeInput {
  short: string;
  /** Aminet directory, e.g. "demo/aga". */
  type: string;
  architecture: string;
  uploaderEmail: string;
  uploaderName: string;
  /** The group the release is signed with: its name and website. */
  group: { label: string; url: string };
  /** The release history creator line, i.e. who made it. */
  entryCreators: string;
  description: string;
  releaseNotesText: string;
  credits: CreditEntry[];
}

export interface AminetFileNames {
  archive: string;
  readme: string;
}

/**
 * What the two files are called on Aminet. The archive keeps its name but
 * in lowercase (Aminet asks for mainly lowercase names), and the readme is
 * the same stem with .readme - Aminet pairs the two by base name.
 */
export function aminetFileNames(archiveBaseName: string): AminetFileNames {
  const archive = archiveBaseName.toLowerCase();
  const stem = archive.replace(/\.[^.]+$/, '');
  return { archive, readme: `${stem}.readme` };
}

const HEADER_LABEL_WIDTH = 'Architecture: '.length;

function headerLine(label: string, value: string): string {
  return `${label}:`.padEnd(HEADER_LABEL_WIDTH) + value.trim();
}

function wrapParagraph(text: string): string[] {
  return wrapWords(text, AMINET_LINE_MAX, AMINET_LINE_MAX);
}

/**
 * Release notes keep the author's own line breaks (they are already laid out
 * for the NFO); only a line that overruns 78 columns is re-wrapped.
 */
function wrapPreformatted(text: string): string[] {
  return text
    .replace(/\r\n?/g, '\n')
    .split('\n')
    .flatMap((line) => (line.length > AMINET_LINE_MAX ? wrapParagraph(line) : [line.replace(/[ \t]+$/, '')]));
}

function trimBlankEdges(lines: string[]): string[] {
  const out = [...lines];
  while (out.length && out[0] === '') out.shift();
  while (out.length && out[out.length - 1] === '') out.pop();
  return out;
}

/** One credit as "Role: Name", the form both the readme and the placeholder renderer print. */
export function formatCreditLine(c: CreditEntry): string {
  return `${c.role.trim()}: ${c.name.trim()}`;
}

export function renderAminetReadme(input: AminetReadmeInput): string {
  const { label, url } = input.group;
  const creators = input.entryCreators.trim();
  const author = creators ? `${label} (${creators})` : label;

  const credits = input.credits.filter((c) => c.role.trim() || c.name.trim()).map((c) => `  ${formatCreditLine(c)}`);

  const body: string[] = [
    ...wrapParagraph(input.description),
    '',
    ...trimBlankEdges(wrapPreformatted(input.releaseNotesText)),
    ...(credits.length ? ['', 'Credits:', ...credits.flatMap((line) => wrapPreformatted(line))] : []),
    '',
    `Released by ${label} - ${url}`,
  ];

  return [
    headerLine('Short', input.short),
    headerLine('Uploader', `${input.uploaderEmail.trim()} (${input.uploaderName.trim()})`),
    headerLine('Author', author),
    headerLine('Type', input.type),
    headerLine('Architecture', input.architecture),
    headerLine('Distribution', 'Aminet'),
    '',
    ...trimBlankEdges(body),
    '',
  ].join('\n');
}

function headerValue(lines: string[], field: string): string | undefined {
  const prefix = `${field}:`;
  const line = lines.find((l) => l.startsWith(prefix));
  return line === undefined ? undefined : line.slice(prefix.length).trim();
}

/**
 * Everything the Aminet upload checklist asks for that can be checked from
 * the text alone. Empty means the readme may go. Both the renderer (to say
 * what is missing before the Publish button does anything) and main (as the
 * last word before the FTP upload) run this on the same text.
 */
export function aminetReadmeProblems(readmeText: string): string[] {
  const problems: string[] = [];
  if (readmeText.includes('\r')) problems.push('the readme must use LF line ends, not CR LF');
  const lines = readmeText.replace(/\r\n?/g, '\n').split('\n');
  const blank = lines.indexOf('');
  const header = blank === -1 ? lines : lines.slice(0, blank);

  if (!lines[0]?.startsWith('Short:')) problems.push('the first line must be "Short:"');
  const short = headerValue(header, 'Short') ?? '';
  if (!short) problems.push('Short: is empty');
  else if (short.length > AMINET_SHORT_MAX) {
    problems.push(`Short: is ${short.length} characters; Aminet allows ${AMINET_SHORT_MAX}`);
  }

  const uploader = headerValue(header, 'Uploader') ?? '';
  if (!/^[^\s@()]+@[^\s@()]+\.[^\s@()]+ \(.+\)$/.test(uploader)) {
    problems.push('Uploader: needs an email address and a name, as "you@example.com (Your Name)"');
  }

  const type = headerValue(header, 'Type') ?? '';
  if (!/^[a-z0-9]+\/[a-z0-9]+$/.test(type)) {
    problems.push('Type: must be an Aminet directory such as "demo/aga"');
  }

  const architecture = headerValue(header, 'Architecture') ?? '';
  if (!architecture) problems.push('Architecture: is empty');

  lines.forEach((line, index) => {
    if (line.length > AMINET_LINE_MAX) {
      problems.push(`line ${index + 1} is ${line.length} characters; Aminet allows ${AMINET_LINE_MAX}`);
    }
  });

  return problems;
}

/** Aminet's file name rules: 30 characters at most, from a small alphabet. */
export function aminetFileNameProblems(names: AminetFileNames): string[] {
  const problems: string[] = [];
  for (const name of [names.archive, names.readme]) {
    if (name.length > AMINET_FILENAME_MAX) {
      problems.push(`"${name}" is ${name.length} characters; Aminet allows ${AMINET_FILENAME_MAX}`);
    }
    if (!/^[A-Za-z0-9._-]+$/.test(name)) {
      problems.push(`"${name}" may only contain letters, digits, dots, underscores and hyphens`);
    }
  }
  return problems;
}
