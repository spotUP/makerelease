import * as fs from 'fs/promises';
import * as path from 'path';
import {
  GroupProfile,
  PROFILE_FILE,
  isProfileId,
  TEMPLATE_FILES,
  parseProfile,
  serializeProfile,
} from '../shared/profile';
import type { ProfileEditRequest } from '../shared/types';
import { normaliseLineEndings } from '../shared/placeholder-template';
import type { ProfileTemplates } from './nfo-placeholder';
import { writeLatin1 } from './encoding';
import { pathExists } from './fs-helpers';
import {
  EXAMPLE_DIZ_TEMPLATE,
  EXAMPLE_NFO_TEMPLATE,
  EXAMPLE_PROFILE_ID,
  EXAMPLE_PROFILE_JSON,
} from './example-profile';

/** A profile folder whose profile.json could not be read or did not validate. */
export interface ProfileLoadError {
  id: string;
  error: string;
}

export function isProfileLoadError(entry: GroupProfile | ProfileLoadError): entry is ProfileLoadError {
  return 'error' in entry;
}

export async function loadProfile(profilesDir: string, id: string): Promise<GroupProfile> {
  const file = path.join(profilesDir, id, PROFILE_FILE);
  // Before any read: an id such as "../x" must never reach the filesystem.
  if (!isProfileId(id)) {
    throw new Error(`${file}: the folder name "${id}" must use only a-z, 0-9 and "-"`);
  }
  let text: string;
  try {
    text = await fs.readFile(file, 'utf8');
  } catch (err) {
    throw new Error(`${file}: could not be read (${(err as Error).message})`);
  }
  let raw: unknown;
  try {
    // Editors on Windows save a UTF-8 BOM, which JSON.parse rejects.
    raw = JSON.parse(text.replace(/^\uFEFF/, ''));
  } catch (err) {
    throw new Error(`${file}: is not valid JSON (${(err as Error).message})`);
  }
  return parseProfile(raw, id, file);
}

/**
 * Every folder under profilesDir, sorted by id. A broken profile is reported
 * in place rather than thrown, so one bad file never hides the others.
 */
export async function listProfiles(profilesDir: string): Promise<Array<GroupProfile | ProfileLoadError>> {
  let entries: import('fs').Dirent[];
  try {
    entries = await fs.readdir(profilesDir, { withFileTypes: true });
  } catch {
    return [];
  }
  const ids = entries
    .filter((entry) => entry.isDirectory() && !entry.name.startsWith('.'))
    .map((entry) => entry.name)
    .sort();
  return Promise.all(
    ids.map(async (id) => {
      try {
        return await loadProfile(profilesDir, id);
      } catch (err) {
        return { id, error: (err as Error).message };
      }
    }),
  );
}

/**
 * The template files of a placeholder profile, read as Latin-1 like the NFOs
 * they become, line endings normalised to LF. release.nfo is optional: a
 * group that ships only a file_id.diz leaves it out.
 */
export async function readTemplates(profilesDir: string, id: string): Promise<ProfileTemplates> {
  const nfo = await readTemplateFile(path.join(profilesDir, id, TEMPLATE_FILES.nfo), true);
  const diz = (await readTemplateFile(path.join(profilesDir, id, TEMPLATE_FILES.diz), false)) as string;
  return { nfo, diz };
}

/** One template file as text; null when `optional` and the file does not exist. */
export async function readTemplateFile(file: string, optional: boolean): Promise<string | null> {
  try {
    return normaliseLineEndings((await fs.readFile(file)).toString('latin1'));
  } catch (err) {
    if (optional && (err as NodeJS.ErrnoException).code === 'ENOENT') return null;
    throw new Error(`${file}: could not be read (${(err as Error).message})`);
  }
}

export type ProfileEditResult = { ok: true } | { ok: false; error: string };

/** profile.json can hold server.writeKey, so it is private to the user and replaced atomically. */
async function writeProfileFile(file: string, text: string): Promise<void> {
  const tmp = `${file}.tmp`;
  await fs.writeFile(tmp, text, { encoding: 'utf8', mode: 0o600 });
  await fs.rename(tmp, file);
}

/**
 * Validates through parseProfile, the single set of rules, before anything
 * is written: a refused save leaves the folder exactly as it was.
 */
export async function saveProfile(profilesDir: string, req: ProfileEditRequest): Promise<ProfileEditResult> {
  const dir = path.join(profilesDir, req.id);
  const file = path.join(dir, PROFILE_FILE);
  try {
    if (!isProfileId(req.id)) throw new Error(`${file}: the folder name "${req.id}" must use only a-z, 0-9 and "-"`);
    const exists = await pathExists(dir);
    if (req.isNew && exists) throw new Error(`A group with id ${req.id} already exists`);
    if (!req.isNew && !exists) throw new Error(`No group with id ${req.id} exists`);
    const text = serializeProfile({ id: req.id, ...req.profile });
    parseProfile(JSON.parse(text), req.id, file);
    await fs.mkdir(dir, { recursive: true });
    await writeProfileFile(file, text);
    if (req.templates) {
      await writeLatin1(path.join(dir, TEMPLATE_FILES.diz), req.templates.diz);
      const nfoFile = path.join(dir, TEMPLATE_FILES.nfo);
      if (req.templates.nfo === null) await fs.rm(nfoFile, { force: true });
      else await writeLatin1(nfoFile, req.templates.nfo);
    }
    return { ok: true };
  } catch (err) {
    return { ok: false, error: (err as Error).message };
  }
}

/** Moves the group's folder to the Trash (recoverable); the last group cannot be removed. */
export async function deleteProfile(
  profilesDir: string,
  id: string,
  trash: (folder: string) => Promise<void>,
): Promise<ProfileEditResult> {
  try {
    if (!isProfileId(id)) throw new Error(`the folder name "${id}" must use only a-z, 0-9 and "-"`);
    const dir = path.join(profilesDir, id);
    if (!(await pathExists(dir))) throw new Error(`No group with id ${id} exists`);
    const entries = await fs.readdir(profilesDir, { withFileTypes: true });
    const folders = entries.filter((entry) => entry.isDirectory() && !entry.name.startsWith('.'));
    if (folders.length <= 1) throw new Error('The last group cannot be deleted');
    await trash(dir);
    return { ok: true };
  } catch (err) {
    return { ok: false, error: (err as Error).message };
  }
}

/**
 * Writes the example profile on first launch only. Once the profiles folder
 * exists it belongs to the user: a deleted example stays deleted.
 */
export async function ensureExampleProfile(profilesDir: string): Promise<void> {
  if (await pathExists(profilesDir)) return;
  const dir = path.join(profilesDir, EXAMPLE_PROFILE_ID);
  await fs.mkdir(dir, { recursive: true });
  await writeProfileFile(path.join(dir, PROFILE_FILE), EXAMPLE_PROFILE_JSON);
  await writeLatin1(path.join(dir, TEMPLATE_FILES.nfo), EXAMPLE_NFO_TEMPLATE);
  await writeLatin1(path.join(dir, TEMPLATE_FILES.diz), EXAMPLE_DIZ_TEMPLATE);
}
