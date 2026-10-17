import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { describe, expect, it } from 'vitest';
import { stageRelease } from '../../src/main/staging';
import { GeneratedOutputs, ReleaseFormData } from '../../src/shared/types';

const generated: GeneratedOutputs = { nfoText: 'nfo text', dizText: 'diz text', releaseNumber: 217 };

const UR = { filePrefix: 'UP-' };
const form = (selectedPaths: string[]): ReleaseFormData => ({
  profileId: 'uprough',
  code: 'EVK26',
  format: 'LHA',
  releaseName: "'EVOKE'",
  platformTag: 'A1200',
  releaseNotesText: 'notes',
  newsText: 'news',
  dizBannerText: 'diz',
  members: [{ nickname: 'spot', role: 'leader' }],
  entryTitle: 'evoke',
  entryCategory: 'Other',
  entryCreators: 'varin0x',
  selectedPaths,
});

async function tree(dir: string): Promise<string[]> {
  const out: string[] = [];
  async function walk(current: string, prefix: string): Promise<void> {
    for (const entry of (await fs.readdir(current, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
      const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
      out.push(entry.isDirectory() ? `${rel}/` : rel);
      if (entry.isDirectory()) await walk(path.join(current, entry.name), rel);
    }
  }
  await walk(dir, '');
  return out;
}

async function tempSource(name: string, files: string[]): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'staging-src-'));
  const dir = path.join(root, name);
  await fs.mkdir(dir, { recursive: true });
  for (const f of files) {
    await fs.mkdir(path.join(dir, path.dirname(f)), { recursive: true });
    await fs.writeFile(path.join(dir, f), 'x');
  }
  return dir;
}

describe('stageRelease', () => {
  it('unwraps a selected folder that is already named UP-<CODE>, instead of nesting it again', async () => {
    const src = await tempSource('UP-EVK26', ['dip-ev26.ans', 'music/varin0x-onmymind.mod']);
    const outerDir = await stageRelease(UR, form([src]), generated);
    expect((await tree(outerDir)).sort()).toEqual([
      'UP-EVK26/',
      'UP-EVK26/UP-EVK26.NFO',
      'UP-EVK26/dip-ev26.ans',
      'UP-EVK26/music/',
      'UP-EVK26/music/varin0x-onmymind.mod',
      'file_id.diz',
    ].sort());
  });

  it('unwraps regardless of how the folder is cased', async () => {
    const src = await tempSource('up-evk26', ['a.txt']);
    const outerDir = await stageRelease(UR, form([src]), generated);
    expect(await tree(outerDir)).toContain('UP-EVK26/a.txt');
    expect(await tree(outerDir)).not.toContain('UP-EVK26/up-evk26/');
  });

  it('keeps any other folder as a folder inside the release', async () => {
    const src = await tempSource('gfx', ['dip-fast.png']);
    const outerDir = await stageRelease(UR, form([src]), generated);
    expect((await tree(outerDir)).sort()).toEqual([
      'UP-EVK26/',
      'UP-EVK26/UP-EVK26.NFO',
      'UP-EVK26/gfx/',
      'UP-EVK26/gfx/dip-fast.png',
      'file_id.diz',
    ].sort());
  });

  it('puts file_id.diz beside the release dir, not inside it', async () => {
    const src = await tempSource('gfx', ['dip-fast.png']);
    const outerDir = await stageRelease(UR, form([src]), generated);
    expect(await fs.readFile(path.join(outerDir, 'file_id.diz'), 'latin1')).toBe('diz text');
  });

  it('writes no NFO when the profile has no NFO template', async () => {
    const src = await tempSource('gfx', ['dip-fast.png']);
    const outerDir = await stageRelease(UR, form([src]), { ...generated, nfoText: null });
    const releaseDir = path.join(outerDir, path.basename(outerDir));
    expect((await fs.readdir(releaseDir)).filter((f) => /\.nfo$/i.test(f))).toEqual([]);
    expect(await fs.readFile(path.join(outerDir, 'file_id.diz'), 'latin1')).toBe('diz text');
  });

  it('copies a selected file straight into the release dir', async () => {
    const dir = await tempSource('loose', ['dip-ev26.ans']);
    const outerDir = await stageRelease(UR, form([path.join(dir, 'dip-ev26.ans')]), generated);
    expect(await tree(outerDir)).toContain('UP-EVK26/dip-ev26.ans');
  });
});
