import { execFile } from 'child_process';
import { promisify } from 'util';

const execFileAsync = promisify(execFile);

// GUI-launched apps (macOS Finder/Dock, Linux AppImage/desktop entries) often
// inherit a minimal PATH that skips these common install locations, even
// though a login shell would find the binary fine. Append them rather than
// hardcoding one absolute path, so any of the standard install locations work.
const EXTRA_PATH_DIRS: Partial<Record<NodeJS.Platform, string[]>> = {
  darwin: ['/opt/homebrew/bin', '/usr/local/bin', '/opt/local/bin'],
  linux: ['/usr/local/bin', '/usr/bin', '/bin'],
};

function envWithExtraPath(): NodeJS.ProcessEnv {
  const extra = EXTRA_PATH_DIRS[process.platform] ?? [];
  if (extra.length === 0) return process.env;
  const sep = process.platform === 'win32' ? ';' : ':';
  return { ...process.env, PATH: `${process.env.PATH ?? ''}${sep}${extra.join(sep)}` };
}

export interface RunExternalOptions {
  cwd?: string;
  /** What to tell the user when the binary is not installed. */
  installHint?: string;
}

/**
 * Runs a command-line tool the app relies on (lha, zip, curl). The one
 * failure every caller handles the same way is the tool not being there at
 * all, so that becomes a readable message with an install hint; anything
 * else is the tool's own error, with its stderr attached by execFile.
 */
export async function runExternal(bin: string, args: string[], options: RunExternalOptions = {}): Promise<void> {
  try {
    await execFileAsync(bin, args, { cwd: options.cwd, env: envWithExtraPath() });
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === 'ENOENT') {
      const hint = options.installHint ?? 'install it and ensure it is on your PATH';
      throw new Error(`"${bin}" was not found on your PATH — ${hint}.`);
    }
    throw err;
  }
}
