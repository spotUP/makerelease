import * as fs from 'fs/promises';
import * as path from 'path';
import { baseName, isDirectory } from './fs-helpers';
import type { ArchivePreview } from '../shared/types';

/** Same rule stageRelease applies: a folder named like the release is merged, not nested. */
export function collapsesIntoRelease(src: string, releaseDir: string, isDir: boolean): boolean {
  return isDir && baseName(src).toUpperCase() === releaseDir.toUpperCase();
}

async function listTree(dir: string, prefix: string, out: string[]): Promise<void> {
  const entries = (await fs.readdir(dir, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name));
  for (const e of entries) {
    const rel = `${prefix}${e.name}`;
    if (e.isDirectory()) {
      out.push(`${rel}/`);
      await listTree(path.join(dir, e.name), `${rel}/`, out);
    } else {
      out.push(rel);
    }
  }
}

/** Follows a folder that holds nothing but one folder, down to the first folder with real content. */
async function innermostSingleChild(src: string): Promise<string> {
  let cur = src;
  for (;;) {
    const entries = await fs.readdir(cur, { withFileTypes: true });
    if (entries.length !== 1 || !entries[0].isDirectory()) return cur;
    cur = path.join(cur, entries[0].name);
  }
}

export async function previewArchive(releaseDir: string, selectedPaths: readonly string[]): Promise<ArchivePreview> {
  const entries = ['file_id.diz', `${releaseDir}/`, `${releaseDir}/${releaseDir}.NFO`];
  const nested: ArchivePreview['nested'] = [];
  const inner = `${releaseDir}/`;
  for (const src of selectedPaths) {
    const isDir = await isDirectory(src);
    if (!isDir) {
      entries.push(`${inner}${baseName(src)}`);
      continue;
    }
    if (collapsesIntoRelease(src, releaseDir, true)) {
      await listTree(src, inner, entries);
    } else {
      entries.push(`${inner}${baseName(src)}/`);
      await listTree(src, `${inner}${baseName(src)}/`, entries);
    }
    const deepest = await innermostSingleChild(src);
    if (deepest !== src) nested.push({ path: src, flattenTo: deepest });
  }
  return { entries, nested };
}
