import * as path from 'path';
import { ReleaseFormat } from '../shared/types';
import type { ProfileSummary } from '../shared/profile';
import { archiveFileName, releaseBaseName } from '../shared/naming';
import { pathExists, moveDir } from './fs-helpers';
import { runExternal } from './external-tool';

const INSTALL_HINTS: Partial<Record<NodeJS.Platform, string>> = {
  darwin: 'install it with "brew install lha" (or "brew install zip")',
  linux: 'install it with your package manager, e.g. "sudo apt install lhasa zip"',
  win32: 'install it and add it to your PATH',
};

async function runArchiver(bin: string, args: string[], cwd: string): Promise<void> {
  await runExternal(bin, args, { cwd, installHint: INSTALL_HINTS[process.platform] });
}

export async function packArchive(
  profile: Pick<ProfileSummary, 'filePrefix'>,
  outerDir: string,
  code: string,
  format: ReleaseFormat,
  makereleaseRoot: string,
): Promise<string> {
  const finalArchivePath = path.join(makereleaseRoot, archiveFileName(profile, code, format));
  const releaseDir = releaseBaseName(profile, code);

  if (format === 'LHA') {
    await runArchiver('lha', ['a', finalArchivePath, 'file_id.diz', releaseDir], outerDir);
  } else {
    await runArchiver('zip', ['-r', finalArchivePath, 'file_id.diz', releaseDir], outerDir);
  }
  return finalArchivePath;
}

export async function finalizeRelease(
  profile: Pick<ProfileSummary, 'filePrefix'>,
  outerDir: string,
  code: string,
  makereleaseRoot: string,
): Promise<string> {
  const releaseDir = releaseBaseName(profile, code);
  const finalDir = path.join(makereleaseRoot, releaseDir);
  if (await pathExists(finalDir)) {
    throw new Error(`${releaseDir} already exists in ${makereleaseRoot} — archive was written, but the staged folder was not moved into place`);
  }
  await moveDir(outerDir, finalDir);
  return finalDir;
}
