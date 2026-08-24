import { GeneratedOutputs, MemberEntry, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
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

export function renderHistoryEntry(entry: ReleaseHistoryEntry): string[] {
  const indent = ' '.repeat(HISTORY_BODY_INDENT);
  const contentWidth = HISTORY_WRAP_WIDTH - HISTORY_CONTINUATION_INDENT;
  const lines: string[] = [`${indent}release ${entry.releaseNumber} ${entry.filenames}`];

  const fields: [string, string][] = [
    ['title', entry.title],
    ['category', entry.category],
    ['creator(s)', entry.creators],
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

export function renderHistoryBlock(history: ReleaseHistoryEntry[]): string {
  return history.map((entry) => renderHistoryEntry(entry).join('\n')).join('\n\n');
}

// `_releaseNumber` is intentionally unused here: `history[0]` already carries the
// just-inserted release's number. The parameter is kept (leading underscore silences
// noUnusedParameters) so generateOutputs' callers can pass it explicitly, documenting
// intent at the call site.
export function renderNfo(history: ReleaseHistoryEntry[], form: ReleaseFormData, _releaseNumber: number): string {
  const releaseNotesLine = `Release named ${form.releaseName}${
    form.platformTag ? ` (${form.platformTag})` : ''
  } (UP-${form.code.toUpperCase()}.${form.format})`;

  const historyBlock = renderHistoryBlock(history);
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

export function generateOutputs(
  history: ReleaseHistoryEntry[],
  releaseNumber: number,
  form: ReleaseFormData,
): GeneratedOutputs {
  return {
    nfoText: renderNfo(history, form, releaseNumber),
    dizText: renderDiz(form),
    releaseNumber,
  };
}
