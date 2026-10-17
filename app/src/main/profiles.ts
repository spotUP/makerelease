import * as fs from 'fs/promises';
import * as path from 'path';
import { GroupProfile, PROFILE_FILE, TEMPLATE_FILES, parseProfile } from '../shared/profile';
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
  let text: string;
  try {
    text = await fs.readFile(file, 'utf8');
  } catch (err) {
    throw new Error(`${file}: could not be read (${(err as Error).message})`);
  }
  let raw: unknown;
  try {
    raw = JSON.parse(text);
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
  const read = async (name: string, optional: boolean): Promise<string | null> => {
    const file = path.join(profilesDir, id, name);
    try {
      return normaliseLineEndings((await fs.readFile(file)).toString('latin1'));
    } catch (err) {
      if (optional && (err as NodeJS.ErrnoException).code === 'ENOENT') return null;
      throw new Error(`${file}: could not be read (${(err as Error).message})`);
    }
  };
  const nfo = await read(TEMPLATE_FILES.nfo, true);
  const diz = (await read(TEMPLATE_FILES.diz, false)) as string;
  return { nfo, diz };
}

/**
 * Writes the example profile on first launch only. Once the profiles folder
 * exists it belongs to the user: a deleted example stays deleted.
 */
export async function ensureExampleProfile(profilesDir: string): Promise<void> {
  if (await pathExists(profilesDir)) return;
  const dir = path.join(profilesDir, EXAMPLE_PROFILE_ID);
  await fs.mkdir(dir, { recursive: true });
  await fs.writeFile(path.join(dir, PROFILE_FILE), EXAMPLE_PROFILE_JSON, 'utf8');
  await writeLatin1(path.join(dir, TEMPLATE_FILES.nfo), EXAMPLE_NFO_TEMPLATE);
  await writeLatin1(path.join(dir, TEMPLATE_FILES.diz), EXAMPLE_DIZ_TEMPLATE);
}
