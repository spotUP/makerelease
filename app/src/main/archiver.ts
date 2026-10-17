import * as path from 'path';
import { ReleaseFormat } from '../shared/types';
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
  outerDir: string,
  code: string,
  format: ReleaseFormat,
  makereleaseRoot: string,
): Promise<string> {
  const ext = format === 'LHA' ? 'LHA' : 'ZIP';
  const finalArchivePath = path.join(makereleaseRoot, `UP-${code.toUpperCase()}.${ext}`);

  if (format === 'LHA') {
    await runArchiver('lha', ['a', finalArchivePath, 'file_id.diz', `UP-${code.toUpperCase()}`], outerDir);
  } else {
    await runArchiver('zip', ['-r', finalArchivePath, 'file_id.diz', `UP-${code.toUpperCase()}`], outerDir);
  }
  return finalArchivePath;
}

export async function finalizeRelease(outerDir: string, code: string, makereleaseRoot: string): Promise<string> {
  const finalDir = path.join(makereleaseRoot, `UP-${code.toUpperCase()}`);
  if (await pathExists(finalDir)) {
    throw new Error(`UP-${code.toUpperCase()} already exists in ${makereleaseRoot} — archive was written, but the staged folder was not moved into place`);
  }
  await moveDir(outerDir, finalDir);
  return finalDir;
}
