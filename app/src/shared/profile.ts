import { isDivisionId, type DivisionId } from './divisions.js';
import type { MemberEntry } from './types';

/**
 * How a group's NFO and DIZ are made: one of the two code renderers that
 * reproduce Up Rough's and /X's hand-drawn layouts, or the placeholder
 * renderer that fills a group's own template files.
 */
export type RendererKind = DivisionId | 'placeholder';

/** Where a release goes after it is built, besides the archive on disk. */
export type BackendKind = 'none' | 'releases-doorserver';

export interface ServerConfig {
  baseUrl: string;
  writeKey: string;
}

export interface GroupProfile {
  /** Folder name under profiles/; [a-z0-9-]+. */
  id: string;
  /** Group name as printed: "Up Rough", "Example Group". */
  label: string;
  /** Printed in the Aminet readme's "Released by" line. */
  url: string;
  /** Prefix of the archive, folder and NFO names: for example "EX-"; "" is allowed. */
  filePrefix: string;
  /** Length of the code typed in the form; 5 for Up Rough. */
  codeLength: number;
  renderer: RendererKind;
  backend: BackendKind;
  /** Present exactly when backend is 'releases-doorserver'. */
  server?: ServerConfig;
  /** Seed for the member list when there is no server to seed it. */
  members: MemberEntry[];
}

/** What the renderer process needs to populate the dropdown and hide sections. */
export interface ProfileSummary {
  id: string;
  label: string;
  /** Printed in the Aminet readme the renderer composes. */
  url: string;
  filePrefix: string;
  codeLength: number;
  renderer: RendererKind;
  backend: BackendKind;
}

/** One dropdown entry: a profile that loaded, or a folder whose profile.json did not. */
export type ProfileListEntry = ProfileSummary | { id: string; error: string };

export const TEMPLATE_FILES = { nfo: 'release.nfo', diz: 'file_id.diz' } as const;

export const PROFILE_FILE = 'profile.json';

const PROFILE_ID = /^[a-z0-9-]+$/;
const RENDERERS: readonly RendererKind[] = ['uprough', 'xinnovations', 'placeholder'];
const BACKENDS: readonly BackendKind[] = ['none', 'releases-doorserver'];

export function isProfileId(value: string): boolean {
  return PROFILE_ID.test(value);
}

export function summarize(profile: GroupProfile): ProfileSummary {
  return {
    id: profile.id,
    label: profile.label,
    url: profile.url,
    filePrefix: profile.filePrefix,
    codeLength: profile.codeLength,
    renderer: profile.renderer,
    backend: profile.backend,
  };
}

/** The division a code renderer stands for, or null for the placeholder renderer. */
export function divisionIdOf(renderer: RendererKind): DivisionId | null {
  return isDivisionId(renderer) ? renderer : null;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

/**
 * Hand-written guard, no schema library: every error names the file and the
 * field so a group editing JSON by hand sees exactly what to fix.
 */
export function parseProfile(raw: unknown, id: string, file: string): GroupProfile {
  const fail = (field: string, problem: string): never => {
    throw new Error(`${file}: "${field}" ${problem}`);
  };
  if (!isProfileId(id)) {
    throw new Error(`${file}: the folder name "${id}" must use only a-z, 0-9 and "-"`);
  }
  if (!isRecord(raw)) throw new Error(`${file}: must be a JSON object`);

  const text = (field: string, required: boolean, fallback = ''): string => {
    const value = raw[field];
    if (value === undefined) {
      if (required) fail(field, 'is missing');
      return fallback;
    }
    if (typeof value !== 'string') return fail(field, 'must be a string');
    if (required && value.trim() === '') fail(field, 'must not be empty');
    return value;
  };

  const label = text('label', true);
  const url = text('url', true);
  const filePrefix = text('filePrefix', false);
  if (/[\\/:*?"<>|\s]/.test(filePrefix)) {
    fail('filePrefix', 'must not contain spaces or any of \\ / : * ? " < > |');
  }

  let codeLength = 5;
  if (raw.codeLength !== undefined) {
    const value = raw.codeLength;
    if (typeof value !== 'number' || !Number.isInteger(value) || value < 1 || value > 32) {
      fail('codeLength', 'must be a whole number from 1 to 32');
    }
    codeLength = value as number;
  }

  const renderer = raw.renderer;
  if (typeof renderer !== 'string' || !(RENDERERS as readonly string[]).includes(renderer)) {
    fail('renderer', `must be one of ${RENDERERS.map((r) => `"${r}"`).join(', ')}`);
  }
  const backend = raw.backend ?? 'none';
  if (typeof backend !== 'string' || !(BACKENDS as readonly string[]).includes(backend)) {
    fail('backend', `must be one of ${BACKENDS.map((b) => `"${b}"`).join(', ')}`);
  }

  let server: ServerConfig | undefined;
  if (backend === 'releases-doorserver') {
    if (renderer === 'placeholder') {
      fail('backend', 'can be "releases-doorserver" only with renderer "uprough" or "xinnovations"');
    }
    if (!isRecord(raw.server)) {
      return fail('server', 'is required when backend is "releases-doorserver"');
    }
    const baseUrl = raw.server.baseUrl;
    const writeKey = raw.server.writeKey ?? '';
    if (typeof baseUrl !== 'string' || baseUrl.trim() === '') fail('server.baseUrl', 'must be a non-empty string');
    if (typeof writeKey !== 'string') fail('server.writeKey', 'must be a string');
    server = { baseUrl: (baseUrl as string).replace(/\/+$/, ''), writeKey: writeKey as string };
  } else if (raw.server !== undefined) {
    fail('server', 'is allowed only when backend is "releases-doorserver"');
  }

  const members: MemberEntry[] = [];
  if (raw.members !== undefined) {
    if (!Array.isArray(raw.members)) fail('members', 'must be a list');
    (raw.members as unknown[]).forEach((entry, index) => {
      if (!isRecord(entry) || typeof entry.nickname !== 'string' || typeof entry.role !== 'string') {
        fail(`members[${index}]`, 'must be an object with string "nickname" and "role"');
      }
      const member = entry as { nickname: string; role: string };
      members.push({ nickname: member.nickname, role: member.role });
    });
  }

  return {
    id,
    label,
    url,
    filePrefix,
    codeLength,
    renderer: renderer as RendererKind,
    backend: backend as BackendKind,
    ...(server ? { server } : {}),
    members,
  };
}
