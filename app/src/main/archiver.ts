import { execFile } from 'child_process';
import { promisify } from 'util';
import * as path from 'path';
import { ReleaseFormat } from '../shared/types';
import { pathExists, moveDir } from './fs-helpers';

const execFileAsync = promisify(execFile);

// GUI-launched apps (macOS Finder/Dock, Linux AppImage/desktop entries) often
// inherit a minimal PATH that skips these common install locations, even
// though a login shell would find the binary fine. Append them rather than
// hardcoding one absolute path, so any of the standard install locations work.
const EXTRA_PATH_DIRS: Partial<Record<NodeJS.Platform, string[]>> = {
  darwin: ['/opt/homebrew/bin', '/usr/local/bin'],
  linux: ['/usr/local/bin', '/usr/bin', '/bin'],
};

function envWithExtraPath(): NodeJS.ProcessEnv {
  const extra = EXTRA_PATH_DIRS[process.platform] ?? [];
  if (extra.length === 0) return process.env;
  const sep = process.platform === 'win32' ? ';' : ':';
  return { ...process.env, PATH: `${process.env.PATH ?? ''}${sep}${extra.join(sep)}` };
}

const INSTALL_HINTS: Partial<Record<NodeJS.Platform, string>> = {
  darwin: 'install it with "brew install lha" (or "brew install zip")',
  linux: 'install it with your package manager, e.g. "sudo apt install lhasa zip"',
  win32: 'install it and add it to your PATH',
};

async function runArchiver(bin: string, args: string[], cwd: string): Promise<void> {
  try {
    await execFileAsync(bin, args, { cwd, env: envWithExtraPath() });
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === 'ENOENT') {
      const hint = INSTALL_HINTS[process.platform] ?? 'install it and ensure it is on your PATH';
      throw new Error(`"${bin}" was not found on your PATH — ${hint}.`);
    }
    throw err;
  }
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
