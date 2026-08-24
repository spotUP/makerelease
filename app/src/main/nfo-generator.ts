import { GeneratedOutputs, MemberEntry, ParsedNfo, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
import {
  BANNER,
  FOOTER_LINE,
  HISTORY_BODY_INDENT,
  HISTORY_CONTINUATION_INDENT,
  HISTORY_WRAP_WIDTH,
  HISTORY_LABEL_FIELD_WIDTH,
  MEMBERS_HEADING,
  MEMBER_LABEL_MIN_WIDTH,
  NEWS_HEADING,
  ONLINE_LINE,
  REL_NOTES_HEADING,
  HISTORY_HEADING,
} from '../shared/constants';
import { toAsciiSafe } from './encoding';

function indentBlock(text: string, indent = HISTORY_BODY_INDENT): string {
  return text
    .split('\n')
    .map((line) => (line.length ? ' '.repeat(indent) + line : line))
    .join('\n');
}

export function renderMemberBlock(members: MemberEntry[]): string {
  const padWidth = Math.max(MEMBER_LABEL_MIN_WIDTH, ...members.map((m) => m.nickname.length + 2));
  return members
    .map((m) => ' '.repeat(HISTORY_BODY_INDENT) + m.nickname.padEnd(padWidth) + m.role)
    .join('\n');
}

function wrapWords(text: string, firstWidth: number, contWidth: number): string[] {
  const words = text.split(/\s+/).filter(Boolean);
  const lines: string[] = [];
  let current = '';
  let width = firstWidth;
  for (const word of words) {
    const candidate = current ? `${current} ${word}` : word;
    if (candidate.length > width && current) {
      lines.push(current);
      current = word;
      width = contWidth;
    } else {
      current = candidate;
    }
  }
  if (current) lines.push(current);
  return lines.length ? lines : [''];
}

export function renderNewHistoryEntry(
  releaseNumber: number,
  filenames: string,
  title: string,
  category: string,
  creators: string,
): string[] {
  const indent = ' '.repeat(HISTORY_BODY_INDENT);
  const contentWidth = HISTORY_WRAP_WIDTH - HISTORY_CONTINUATION_INDENT;
  const lines: string[] = [`${indent}release ${releaseNumber} ${filenames}`];

  const fields: [string, string][] = [
    ['title', title],
    ['category', category],
    ['creator(s)', creators],
  ];
  for (const [label, value] of fields) {
    const wrapped = wrapWords(value, contentWidth, contentWidth);
    lines.push(`${indent}${label.padEnd(HISTORY_LABEL_FIELD_WIDTH)}${wrapped[0]}`);
    for (let i = 1; i < wrapped.length; i++) {
      lines.push(' '.repeat(HISTORY_CONTINUATION_INDENT) + wrapped[i]);
    }
  }
  return lines;
}

export function renderHistoryBlock(newEntryLines: string[], existing: ReleaseHistoryEntry[]): string {
  const blocks = [newEntryLines.join('\n'), ...existing.map((e) => e.rawBlock.join('\n'))];
  return blocks.join('\n\n');
}

export function renderNfo(parsed: ParsedNfo, form: ReleaseFormData, releaseNumber: number): string {
  const filenames = `up-${form.code.toLowerCase()}.${form.format.toLowerCase()}`;
  const releaseNotesLine = `Release named ${form.releaseName}${
    form.platformTag ? ` (${form.platformTag})` : ''
  } (UP-${form.code.toUpperCase()}.${form.format})`;

  const newEntry = renderNewHistoryEntry(releaseNumber, filenames, form.entryTitle, form.entryCategory, form.entryCreators);
  const historyBlock = renderHistoryBlock(newEntry, parsed.history);
  const membersBlock = renderMemberBlock(form.members);

  return [
    BANNER,
    '',
    REL_NOTES_HEADING,
    '',
    ' '.repeat(HISTORY_BODY_INDENT) + releaseNotesLine,
    '',
    indentBlock(form.releaseNotesText.trim()),
    '',
    NEWS_HEADING,
    '',
    indentBlock(form.newsText.trim()),
    '',
    MEMBERS_HEADING,
    '',
    membersBlock,
    '',
    '',
    ONLINE_LINE,
    '',
    '',
    HISTORY_HEADING,
    '',
    historyBlock,
    '',
    '',
    FOOTER_LINE,
    '',
  ].join('\n');
}

export function renderDiz(form: ReleaseFormData): string {
  return toAsciiSafe(form.dizBannerText.trim()) + '\n';
}

export function generateOutputs(parsed: ParsedNfo, form: ReleaseFormData): GeneratedOutputs {
  const releaseNumber = parsed.maxReleaseNumber + 1;
  return {
    nfoText: renderNfo(parsed, form, releaseNumber),
    dizText: renderDiz(form),
    releaseNumber,
  };
}
