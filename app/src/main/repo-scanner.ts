import * as fs from 'fs/promises';
import * as path from 'path';
import { ParsedNfo } from '../shared/types';
import { parseNfo } from './nfo-parser';
import { readLatin1 } from './encoding';

const CODE_DIR_RE = /^UP-[A-Za-z0-9]{5}$/;

async function findNfoPath(root: string, releaseDirName: string): Promise<string | null> {
  const candidate = path.join(root, releaseDirName, releaseDirName, `${releaseDirName}.NFO`);
  try {
    await fs.access(candidate);
    return candidate;
  } catch {
    return null;
  }
}

export async function scanForLatestNfo(root: string): Promise<ParsedNfo> {
  const entries = await fs.readdir(root, { withFileTypes: true });
  const releaseDirs = entries.filter((e) => e.isDirectory() && CODE_DIR_RE.test(e.name)).map((e) => e.name);

  let best: ParsedNfo | null = null;
  for (const dirName of releaseDirs) {
    const nfoPath = await findNfoPath(root, dirName);
    if (!nfoPath) continue;
    try {
      const text = await readLatin1(nfoPath);
      const parsed = parseNfo(text, nfoPath);
      if (!best || parsed.maxReleaseNumber > best.maxReleaseNumber) {
        best = parsed;
      }
    } catch (err) {
      console.warn('repo-scanner: failed to parse', nfoPath, err);
    }
  }

  if (!best) {
    throw new Error(`repo-scanner: no parseable UP-XXXXX release NFO found under ${root}`);
  }
  return best;
}

export async function codeExists(root: string, code: string): Promise<boolean> {
  const dirPath = path.join(root, `UP-${code.toUpperCase()}`);
  const lhaPath = path.join(root, `UP-${code.toUpperCase()}.LHA`);
  const zipPath = path.join(root, `UP-${code.toUpperCase()}.ZIP`);
  for (const p of [dirPath, lhaPath, zipPath]) {
    try {
      await fs.access(p);
      return true;
    } catch {
      /* not found, keep checking */
    }
  }
  return false;
}
