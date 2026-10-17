import { renderXInnovationsNfo } from './nfo-x-innovations';
import { GeneratedOutputs, MemberEntry, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
import type { GroupProfile, ProfileSummary } from '../shared/profile';
import { renderPlaceholderOutputs, type ProfileTemplates } from './nfo-placeholder';
import { archiveFileName } from '../shared/naming';
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
import { wrapWords } from '../shared/text-wrap';

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
export function renderNfo(
  profile: Pick<ProfileSummary, 'filePrefix'>,
  history: ReleaseHistoryEntry[],
  form: ReleaseFormData,
  _releaseNumber: number,
): string {
  const releaseNotesLine = `Release named ${form.releaseName}${
    form.platformTag ? ` (${form.platformTag})` : ''
  } (${archiveFileName(profile, form.code, form.format)})`;

  // Under the "Release named ..." line and above the notes: the two
  // together say what this release is and when it came out, which is what a
  // trader reads first. Releases built before the field existed simply have
  // no such line, rather than a made-up date.
  const releaseDateLine = form.releaseDate?.trim()
    ? ' '.repeat(HISTORY_BODY_INDENT) + `Released ${form.releaseDate.trim()}`
    : null;

  const historyBlock = renderHistoryBlock(history);
  const membersBlock = renderMemberBlock(form.members);

  return [
    BANNER,
    '',
    REL_NOTES_HEADING,
    '',
    ' '.repeat(HISTORY_BODY_INDENT) + releaseNotesLine,
    ...(releaseDateLine ? [releaseDateLine] : []),
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

// The DIZ grid pads every row to DIZ_COLS, so trailing blanks are padding, not
// art. Leading spaces on a line ARE art (indented ASCII) and must survive.
// Exported because the catalog copy of the DIZ (sent by releases-client) has
// to be trimmed exactly the same way the file_id.diz is - otherwise the
// release page on the web would render the grid's 45-column padding.
export function trimDizBanner(text: string): string {
  const lines = text.split('\n').map((line) => line.replace(/[ \t]+$/, ''));
  while (lines.length && lines[0] === '') lines.shift();
  while (lines.length && lines[lines.length - 1] === '') lines.pop();
  return lines.join('\n');
}

export function renderDiz(form: ReleaseFormData): string {
  return toAsciiSafe(trimDizBanner(form.dizBannerText)) + '\n';
}

/**
 * Which renderer runs is decided here, once, from the profile: Up Rough's
 * plain 80-column layout, /X Innovations' framed drawing, or a group's own
 * template files with placeholders filled in.
 *
 * `releaseList` is only needed by /X, whose NFO lists every previous release
 * as its own banner art rather than as a line of text. `templates` is only
 * needed by the placeholder renderer.
 */
export function generateOutputs(
  profile: Pick<GroupProfile, 'filePrefix' | 'label' | 'url' | 'renderer'>,
  history: ReleaseHistoryEntry[],
  releaseNumber: number,
  form: ReleaseFormData,
  releaseList: ReadonlyArray<{ displayNumber: string; art: string }> = [],
  templates?: ProfileTemplates,
): GeneratedOutputs {
  if (profile.renderer === 'placeholder') {
    if (!templates) throw new Error(`${profile.label}: the placeholder renderer needs the profile's template files`);
    return renderPlaceholderOutputs(profile, templates, form);
  }
  const dizText = renderDiz(form);
  if (profile.renderer === 'xinnovations') {
    return {
      nfoText: renderXInnovationsNfo({
        releaseName: form.entryTitle,
        releaseDate: form.releaseDate ?? '',
        coder: form.coder ?? '',
        system: form.system ?? '',
        design: form.design ?? '',
        releaseNotes: form.releaseNotesText,
        news: form.newsText,
        members: form.members,
        boards: form.boards ?? [],
        releaseList,
      }),
      dizText,
      releaseNumber,
    };
  }
  return {
    nfoText: renderNfo(profile, history, form, releaseNumber),
    dizText,
    releaseNumber,
  };
}
