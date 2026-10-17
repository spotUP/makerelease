import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';

// Reachability: a group made, built from and deleted through the editor's
// own IPC channels, against a real folder. Only Electron is replaced.
const handlers: Record<string, (...args: any[]) => any> = {};
// Stands in for the system Trash: the folder leaves the profiles folder.
const trashItem = vi.fn(async (folder: string) => fs.rm(folder, { recursive: true }));
vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
  shell: { openPath: vi.fn(), trashItem: (...args: unknown[]) => (trashItem as any)(...args) },
}));

import * as electron from 'electron';
import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import { ensureExampleProfile } from '../../src/main/profiles';
import { formToRequest, profileToForm } from '../../src/renderer/profile-editor';
import type { ReleaseFormData } from '../../src/shared/types';

let userData: string;

beforeEach(async () => {
  userData = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-userdata-'));
  trashItem.mockClear();
});

afterEach(async () => {
  await fs.rm(userData, { recursive: true, force: true });
});

describe('the group editor reaches the window', () => {
  it('a group saved from a draft is listed, seeded, previewed with its imported template, then trashed', async () => {
    const profilesDir = path.join(userData, 'profiles');
    await ensureExampleProfile(profilesDir);
    registerIpcHandlers(profilesDir, () => path.join(userData, 'out'));

    const draft = await handlers.newProfileDraft(null);
    (electron.dialog.showOpenDialog as any).mockResolvedValueOnce({ canceled: false, filePaths: [path.join(userData, 'art.diz')] });
    await fs.writeFile(path.join(userData, 'art.diz'), 'SENTINEL-DIZ {{release_name}}\r\n', 'latin1');
    const imported = await handlers.importTemplate(null);

    const form = profileToForm(draft, 'new', ['example']);
    form.label = 'SENTINEL Group';
    form.id = 'sentinel-group';
    form.url = 'https://sentinel.net';
    form.filePrefix = 'SENTINEL-';
    form.templates.diz = imported.text;
    expect(await handlers.saveProfile(null, formToRequest(form))).toEqual({ ok: true });

    const listed = await handlers.listProfiles(null);
    expect(listed.map((entry: { id: string }) => entry.id)).toContain('sentinel-group');
    expect((await handlers.loadSeedState(null, 'sentinel-group')).profile.filePrefix).toBe('SENTINEL-');

    const releaseForm: ReleaseFormData = {
      profileId: 'sentinel-group', code: 'ABCDE', format: 'LHA', releaseName: 'Rel', platformTag: '',
      releaseNotesText: 'n', newsText: '', dizBannerText: '', members: [], entryTitle: '', entryCategory: '',
      entryCreators: '', selectedPaths: [], description: 'd', credits: [], mediaEmbeds: [],
    };
    const preview = await handlers.previewOutputs(null, releaseForm);
    expect(preview.dizText).toContain('SENTINEL-DIZ');

    expect(await handlers.deleteProfile(null, 'sentinel-group')).toEqual({ ok: true });
    expect(trashItem).toHaveBeenCalledTimes(1);
    expect(trashItem).toHaveBeenCalledWith(path.join(profilesDir, 'sentinel-group'));
    const after = await handlers.listProfiles(null);
    expect(after.map((entry: { id: string }) => entry.id)).not.toContain('sentinel-group');
  });
});
