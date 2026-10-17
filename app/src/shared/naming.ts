import type { ProfileSummary } from './profile';
import type { ReleaseFormat } from './types';

type Prefixed = Pick<ProfileSummary, 'filePrefix'>;

/**
 * The name every release artefact shares: the outer and inner folders, the
 * NFO and the archive. One function so the archiver, the stager, the NFO and
 * the form's hint can never disagree.
 */
export function releaseBaseName(profile: Prefixed, code: string): string {
  return `${profile.filePrefix}${code.toUpperCase()}`;
}

export function archiveFileName(profile: Prefixed, code: string, format: ReleaseFormat): string {
  return `${releaseBaseName(profile, code)}.${format === 'LHA' ? 'LHA' : 'ZIP'}`;
}
