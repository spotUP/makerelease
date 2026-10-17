import type { GeneratedOutputs, ReleaseFormData } from '../shared/types';
import { TEMPLATE_FILES, type GroupProfile } from '../shared/profile';
import { archiveFileName } from '../shared/naming';
import { formatCreditLine } from '../shared/aminet-readme';
import {
  TemplateError,
  renderTemplate,
  templateProblems,
  type TemplateValues,
} from '../shared/placeholder-template';

/** A placeholder profile's template files; a profile without release.nfo ships no NFO. */
export interface ProfileTemplates {
  nfo: string | null;
  diz: string;
}

export type PlaceholderProfile = Pick<GroupProfile, 'filePrefix' | 'label' | 'url'>;

/** A textarea as template lines: LF only, no trailing whitespace, no blank edges. */
function textLines(text: string): string[] {
  const lines = text.replace(/\r\n?/g, '\n').split('\n').map((line) => line.replace(/\s+$/, ''));
  while (lines.length && lines[0] === '') lines.shift();
  while (lines.length && lines[lines.length - 1] === '') lines.pop();
  return lines;
}

export function placeholderValues(profile: PlaceholderProfile, form: ReleaseFormData): TemplateValues {
  const width = Math.max(0, ...form.members.map((m) => m.nickname.length)) + 2;
  return {
    scalars: {
      group: profile.label,
      url: profile.url,
      code: form.code.toUpperCase(),
      filename: archiveFileName(profile, form.code, form.format),
      format: form.format,
      release_name: form.releaseName,
      platform: form.platformTag,
      date: form.releaseDate ?? '',
      release_number: form.releaseNumberText ?? '',
      description: form.description,
      creators: form.entryCreators,
    },
    blocks: {
      notes: textLines(form.releaseNotesText),
      news: textLines(form.newsText),
      members: form.members.map((m) => m.nickname.padEnd(width) + m.role),
      credits: form.credits.filter((c) => c.role.trim() || c.name.trim()).map(formatCreditLine),
    },
  };
}

function renderFile(file: string, template: string, values: TemplateValues): string {
  try {
    const text = renderTemplate(template, values);
    return text.endsWith('\n') ? text : `${text}\n`;
  } catch (err) {
    if (err instanceof TemplateError) throw new Error(`${file} line ${err.line}: ${err.message}`);
    throw err;
  }
}

/** Fills a group's own templates. Nothing is numbered: the number is whatever was typed. */
export function renderPlaceholderOutputs(
  profile: PlaceholderProfile,
  templates: ProfileTemplates,
  form: ReleaseFormData,
): GeneratedOutputs {
  const values = placeholderValues(profile, form);
  return {
    nfoText: templates.nfo === null ? null : renderFile(TEMPLATE_FILES.nfo, templates.nfo, values),
    dizText: renderFile(TEMPLATE_FILES.diz, templates.diz, values),
    releaseNumber: 0,
  };
}

/** Every template problem for the window's preview, each prefixed with its file. */
export function placeholderProblems(
  profile: PlaceholderProfile,
  templates: ProfileTemplates,
  form: ReleaseFormData,
): string[] {
  const values = placeholderValues(profile, form);
  const of = (file: string, template: string | null) =>
    template === null ? [] : templateProblems(template, values).map((p) => `${file} ${p}`);
  return [...of(TEMPLATE_FILES.nfo, templates.nfo), ...of(TEMPLATE_FILES.diz, templates.diz)];
}
