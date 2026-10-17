import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';

// Reachability: a server-less profile builds end to end through the same
// startup calls index.ts makes and the window's own IPC calls. Only Electron
// and the external archiver binary are replaced; any network call fails the test.
const handlers: Record<string, (...args: any[]) => any> = {};
vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
  shell: { openPath: vi.fn() },
}));

const runExternal = vi.fn(async (_bin: string, args: string[]) => {
  // Stands in for lha: writes the archive it was asked for.
  await fs.writeFile(args[1], 'archive', 'utf8');
});
vi.mock('../../src/main/external-tool', () => ({ runExternal: (...a: [string, string[]]) => runExternal(...a) }));

import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import { ensureExampleProfile } from '../../src/main/profiles';

let userData: string;
const realFetch = global.fetch;
const fetchSpy = vi.fn(async () => {
  throw new Error('a server-less profile must not touch the network');
});

beforeEach(async () => {
  userData = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-userdata-'));
  global.fetch = fetchSpy as unknown as typeof fetch;
  fetchSpy.mockClear();
  runExternal.mockClear();
});

afterEach(async () => {
  global.fetch = realFetch;
  await fs.rm(userData, { recursive: true, force: true });
});

describe('a server-less profile reaches a finished build', () => {
  it('builds the example profile without a single network call', async () => {
    const profilesDir = path.join(userData, 'profiles');
    const outputRoot = path.join(userData, 'out');
    await ensureExampleProfile(profilesDir);
    registerIpcHandlers(profilesDir, () => outputRoot, {});

    const seed = await handlers.loadSeedState(null, 'example');
    expect(seed.nextReleaseNumber).toBe(0);
    expect(await handlers.checkCodeAvailability(null, 'example', 'TEST1')).toEqual({ exists: false });

    const form = {
      profileId: 'example', code: 'TEST1', format: 'LHA', releaseName: "'SENTINEL RELEASE'",
      platformTag: '', releaseNotesText: 'notes', newsText: '', dizBannerText: seed.currentDizText,
      members: seed.members, entryTitle: '', entryCategory: '', entryCreators: '', selectedPaths: [],
      description: 'd', credits: [], mediaEmbeds: [], releaseDate: '2026-10-17',
    };
    const result = await handlers.buildRelease(null, form, false);

    expect(fetchSpy).toHaveBeenCalledTimes(0);
    expect(runExternal).toHaveBeenCalledTimes(1);
    expect(result.releaseNumber).toBe(0);
    expect(result.archivePath).toBe(path.join(outputRoot, 'EX-TEST1.LHA'));
    await expect(fs.stat(path.join(outputRoot, 'EX-TEST1', 'file_id.diz'))).resolves.toBeTruthy();
  });
});
