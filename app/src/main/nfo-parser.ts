import { MemberEntry, ParsedNfo, ReleaseHistoryEntry } from '../shared/types';
import { HISTORY_BODY_INDENT, HISTORY_CONTINUATION_INDENT } from '../shared/constants';

function stripIndent(line: string, indent: number): string {
  return line.startsWith(' '.repeat(indent)) ? line.slice(indent) : line.trimStart();
}

function findHeadingIndex(lines: string[], needle: string, from = 0): number {
  for (let i = from; i < lines.length; i++) {
    if (lines[i].includes(needle)) return i;
  }
  return -1;
}

function collectTextBlock(lines: string[], start: number, end: number): string {
  return lines
    .slice(start, end)
    .map((l) => stripIndent(l, HISTORY_BODY_INDENT))
    .join('\n')
    .replace(/^\n+/, '')
    .replace(/\n+$/, '');
}

function parseReleaseNotesLine(line: string): string {
  const match = line.match(/^\s*Release named (.*?)\s*\(UP-[^)]+\.(?:LHA|ZIP)\)\s*$/i);
  if (match) return match[1].trim();
  console.warn('nfo-parser: could not cleanly parse release-notes title line, storing raw:', line);
  return line.trim();
}

function parseMembers(lines: string[], start: number, end: number): MemberEntry[] {
  const members: MemberEntry[] = [];
  const memberRe = /^ {8}(\S(?:.*\S)?)\s{2,}(\S.*?)\s*$/;
  for (let i = start; i < end; i++) {
    const line = lines[i];
    if (!line.trim()) continue;
    const match = line.match(memberRe);
    if (match) {
      members.push({ nickname: match[1], role: match[2] });
    } else {
      console.warn('nfo-parser: skipping unparseable member line:', JSON.stringify(line));
    }
  }
  return members;
}

function isHistoryEntryStart(line: string): RegExpMatchArray | null {
  return line.match(/^ {8}release (\S+) (.*)$/);
}

function parseHistory(lines: string[], start: number, end: number): ReleaseHistoryEntry[] {
  const entries: ReleaseHistoryEntry[] = [];
  let i = start;
  while (i < end) {
    const line = lines[i];
    const startMatch = isHistoryEntryStart(line);
    if (!startMatch) {
      if (line.trim()) {
        console.warn('nfo-parser: skipping stray line before history entry:', JSON.stringify(line));
      }
      i++;
      continue;
    }
    const rawBlock: string[] = [line];
    const releaseNumberRaw = startMatch[1];
    const filenames = startMatch[2];
    let title = '';
    let category = '';
    let creators = '';
    let currentField: 'title' | 'category' | 'creators' | null = null;
    i++;
    while (i < end && lines[i].trim() !== '') {
      const fieldLine = lines[i];
      rawBlock.push(fieldLine);
      const labelMatch = fieldLine.match(/^ {8}(title|category|creator\(s\))\s+(.*)$/);
      if (labelMatch) {
        const value = labelMatch[2].trim();
        if (labelMatch[1] === 'title') {
          title = value;
          currentField = 'title';
        } else if (labelMatch[1] === 'category') {
          category = value;
          currentField = 'category';
        } else {
          creators = value;
          currentField = 'creators';
        }
      } else if (fieldLine.startsWith(' '.repeat(HISTORY_CONTINUATION_INDENT)) && currentField) {
        const cont = fieldLine.slice(HISTORY_CONTINUATION_INDENT).trim();
        if (currentField === 'title') title += ' ' + cont;
        else if (currentField === 'category') category += ' ' + cont;
        else creators += ' ' + cont;
      } else {
        console.warn('nfo-parser: unrecognized history field line, kept in rawBlock only:', JSON.stringify(fieldLine));
      }
      i++;
    }
    const releaseNumberValue = /^\d+$/.test(releaseNumberRaw) ? parseInt(releaseNumberRaw, 10) : null;
    entries.push({ releaseNumberRaw, releaseNumberValue, filenames, title, category, creators, rawBlock });
  }
  return entries;
}

export function parseNfo(text: string, sourceFilePath: string): ParsedNfo {
  const lines = text.split('\n');

  const relNotesIdx = findHeadingIndex(lines, '[ r e l e a s e   n o t e s ]');
  const newsIdx = findHeadingIndex(lines, '[ n e w s ]', relNotesIdx + 1);
  const membersIdx = findHeadingIndex(lines, 'a c t i v e   u p   r o u g h   h e r o e s', newsIdx + 1);
  const onlineIdx = findHeadingIndex(lines, 'online', membersIdx + 1);
  const historyIdx = findHeadingIndex(lines, 'r e l e a s e d   s o   f a r', onlineIdx + 1);

  if (relNotesIdx === -1 || newsIdx === -1 || membersIdx === -1 || historyIdx === -1) {
    throw new Error(`nfo-parser: could not locate all required section headings in ${sourceFilePath}`);
  }

  let titleLineIdx = relNotesIdx + 1;
  while (titleLineIdx < newsIdx && !lines[titleLineIdx].trim()) titleLineIdx++;
  const currentReleaseNotesTitleLine = parseReleaseNotesLine(lines[titleLineIdx]);

  const currentReleaseNotesText = collectTextBlock(lines, titleLineIdx + 1, newsIdx);
  const currentNewsText = collectTextBlock(lines, newsIdx + 1, membersIdx);

  const members = parseMembers(lines, membersIdx + 1, onlineIdx);

  let historyEnd = lines.length;
  const footerIdx = findHeadingIndex(lines, 'c a t c h', historyIdx + 1);
  if (footerIdx !== -1) historyEnd = footerIdx;
  const history = parseHistory(lines, historyIdx + 1, historyEnd);

  const maxReleaseNumber = history.reduce(
    (max, entry) => (entry.releaseNumberValue !== null ? Math.max(max, entry.releaseNumberValue) : max),
    0,
  );

  return {
    sourceFilePath,
    currentReleaseNotesTitleLine,
    currentReleaseNotesText,
    currentNewsText,
    members,
    history,
    maxReleaseNumber,
  };
}
