import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';

// Reachability: the same two calls index.ts makes on startup
// (ensureExampleProfile, then registerIpcHandlers), against a real folder,
// then the dropdown's own IPC call. Only Electron is replaced.
const handlers: Record<string, (...args: any[]) => any> = {};
vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
  shell: { openPath: vi.fn() },
}));

import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import { ensureExampleProfile } from '../../src/main/profiles';
import { EXAMPLE_DIZ_TEMPLATE } from '../../src/main/example-profile';

let userData: string;

beforeEach(async () => {
  userData = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-userdata-'));
});

afterEach(async () => {
  await fs.rm(userData, { recursive: true, force: true });
});

describe('group profiles reach the window', () => {
  it('a first launch lists the example profile read from disk, and an edit to profile.json shows up on reload', async () => {
    const profilesDir = path.join(userData, 'profiles');
    await ensureExampleProfile(profilesDir);
    registerIpcHandlers(profilesDir, () => path.join(userData, 'out'));

    const first = await handlers.listProfiles(null);
    expect(first).toEqual([
      { id: 'example', label: 'Example Group', url: 'https://example.net', filePrefix: 'EX-', codeLength: 5, renderer: 'placeholder', backend: 'none' },
    ]);
    expect(await fs.readFile(path.join(profilesDir, 'example', 'file_id.diz'))).toEqual(
      Buffer.from(EXAMPLE_DIZ_TEMPLATE, 'latin1'),
    );

    // Sentinel: a value that exists only on disk proves the handler reads the file.
    const file = path.join(profilesDir, 'example', 'profile.json');
    const json = JSON.parse(await fs.readFile(file, 'utf8'));
    await fs.writeFile(file, JSON.stringify({ ...json, filePrefix: 'SENTINEL-' }), 'utf8');

    const seed = await handlers.loadSeedState(null, 'example');
    expect(seed.profile.filePrefix).toBe('SENTINEL-');
    expect(seed.outputRoot).toBe(path.join(userData, 'out'));
    expect((await handlers.listProfiles(null))[0].filePrefix).toBe('SENTINEL-');
  });
});
