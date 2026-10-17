import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';

// Reachability: a group's own release.nfo and file_id.diz reach the archive
// through the window's real IPC calls. Only Electron and the external
// archiver binary are replaced; the renderer itself is the real one.
const handlers: Record<string, (...args: any[]) => any> = {};
vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
  shell: { openPath: vi.fn() },
}));

const runExternal = vi.fn(async (_bin: string, args: string[]) => {
  await fs.writeFile(args[1], 'archive', 'utf8');
});
vi.mock('../../src/main/external-tool', () => ({ runExternal: (...a: [string, string[]]) => runExternal(...a) }));

import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import { ensureExampleProfile } from '../../src/main/profiles';
import { EX_FORM } from './ex-placeholder.fixture';

const SENTINEL = 'SENTINEL-7F3A';
let userData: string;
let profilesDir: string;
let outputRoot: string;

beforeEach(async () => {
  userData = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-placeholder-'));
  profilesDir = path.join(userData, 'profiles');
  outputRoot = path.join(userData, 'out');
  await ensureExampleProfile(profilesDir);
  registerIpcHandlers(profilesDir, () => outputRoot, {});
  runExternal.mockClear();
});

afterEach(async () => {
  await fs.rm(userData, { recursive: true, force: true });
});

const outer = (...parts: string[]) => path.join(outputRoot, 'dry-run', 'EX-TEST1', ...parts);
const inner = (...parts: string[]) => outer('EX-TEST1', ...parts);

describe('a placeholder profile reaches a finished build', () => {
  it('fills the profile release.nfo and file_id.diz into a dry-run archive', async () => {
    await fs.writeFile(path.join(profilesDir, 'example', 'release.nfo'), `${SENTINEL} {{group}}\r\n`, 'latin1');

    const result = await handlers.buildRelease(null, EX_FORM, true);

    expect(runExternal).toHaveBeenCalledTimes(1);
    expect(result.archivePath).toBe(path.join(outputRoot, 'dry-run', 'EX-TEST1.LHA'));
    expect(await fs.readFile(inner('EX-TEST1.NFO'), 'latin1')).toBe(`${SENTINEL} Example Group\n`);
    const diz = await fs.readFile(outer('file_id.diz'), 'latin1');
    expect(diz).toContain('Demo Disk');
    expect(new Set(diz.split('\n').filter((l) => l.startsWith('|')).map((l) => l.length))).toEqual(new Set([46]));
  });

  it('builds without an NFO when the profile has no release.nfo', async () => {
    await fs.rm(path.join(profilesDir, 'example', 'release.nfo'));

    await handlers.buildRelease(null, EX_FORM, true);

    expect(runExternal).toHaveBeenCalledTimes(1);
    expect((await fs.readdir(inner())).filter((f) => /\.nfo$/i.test(f))).toEqual([]);
    expect(await fs.readdir(outer())).toContain('file_id.diz');
  });

  it('previews the DIZ from the template on disk and reports a missing NFO', async () => {
    await fs.rm(path.join(profilesDir, 'example', 'release.nfo'));

    const preview = await handlers.previewOutputs(null, EX_FORM);

    expect(preview.hasNfoTemplate).toBe(false);
    expect(preview.nfoText).toBeNull();
    expect(preview.dizText).toContain('Demo Disk');
    expect(preview.problems).toEqual([]);
  });

  it('previews a template problem with its file and line instead of rendering', async () => {
    await fs.writeFile(path.join(profilesDir, 'example', 'release.nfo'), 'fine\n{{bogus}}\n', 'latin1');

    const preview = await handlers.previewOutputs(null, EX_FORM);

    expect(preview.problems).toEqual(['release.nfo line 2: unknown placeholder {{bogus}}']);
    await expect(handlers.buildRelease(null, EX_FORM, true)).rejects.toThrow(
      'release.nfo line 2: unknown placeholder {{bogus}}',
    );
  });
});
