import { execFile } from 'child_process';
import { promisify } from 'util';
import * as path from 'path';
import { ReleaseFormat } from '../shared/types';
import { pathExists, moveDir } from './fs-helpers';

const execFileAsync = promisify(execFile);

const LHA_BIN = '/opt/homebrew/bin/lha';
const ZIP_BIN = '/usr/bin/zip';

export async function packArchive(
  outerDir: string,
  code: string,
  format: ReleaseFormat,
  makereleaseRoot: string,
): Promise<string> {
  const ext = format === 'LHA' ? 'LHA' : 'ZIP';
  const finalArchivePath = path.join(makereleaseRoot, `UP-${code.toUpperCase()}.${ext}`);

  if (format === 'LHA') {
    await execFileAsync(LHA_BIN, ['a', finalArchivePath, 'file_id.diz', `UP-${code.toUpperCase()}`], {
      cwd: outerDir,
    });
  } else {
    await execFileAsync(ZIP_BIN, ['-r', finalArchivePath, 'file_id.diz', `UP-${code.toUpperCase()}`], {
      cwd: outerDir,
    });
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
