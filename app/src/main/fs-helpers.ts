import * as fs from 'fs/promises';
import * as path from 'path';

export async function copyRecursive(src: string, dest: string): Promise<void> {
  await fs.cp(src, dest, { recursive: true });
}

export async function pathExists(p: string): Promise<boolean> {
  try {
    await fs.access(p);
    return true;
  } catch {
    return false;
  }
}

export async function ensureDir(p: string): Promise<void> {
  await fs.mkdir(p, { recursive: true });
}

/** Removes a file or a whole folder; a path that is not there is not an error. */
export async function removePath(p: string): Promise<void> {
  await fs.rm(p, { recursive: true, force: true });
}

export async function moveDir(src: string, dest: string): Promise<void> {
  try {
    await fs.rename(src, dest);
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === 'EXDEV') {
      await fs.cp(src, dest, { recursive: true });
      await fs.rm(src, { recursive: true, force: true });
    } else {
      throw err;
    }
  }
}

export async function isDirectory(p: string): Promise<boolean> {
  try {
    return (await fs.stat(p)).isDirectory();
  } catch {
    return false;
  }
}

export function baseName(p: string): string {
  return path.basename(p);
}
