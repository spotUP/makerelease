import { describe, it, expect, vi, beforeEach } from 'vitest';

const handlers: Record<string, (...args: any[]) => any> = {};

vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
}));

vi.mock('../../src/main/releases-client', () => {
  class ReleasesApiError extends Error {
    status?: number;
    wordpressPostUrl?: string;
    constructor(message: string, opts?: { status?: number; wordpressPostUrl?: string }) {
      super(message);
      this.status = opts?.status;
      this.wordpressPostUrl = opts?.wordpressPostUrl;
    }
  }
  return {
    fetchSeed: vi.fn(),
    fetchNumberingCheck: vi.fn(),
    checkCodeExists: vi.fn(),
    publishRelease: vi.fn(),
    deleteRelease: vi.fn(),
    publishToWeb: vi.fn(),
    ReleasesApiError,
  };
});

vi.mock('../../src/main/nfo-generator', () => ({
  generateOutputs: vi.fn(),
}));

vi.mock('../../src/main/staging', () => ({
  stageRelease: vi.fn(),
}));

vi.mock('../../src/main/archiver', () => ({
  packArchive: vi.fn(),
  finalizeRelease: vi.fn(),
}));

vi.mock('../../src/main/fs-helpers', () => ({
  pathExists: vi.fn(),
}));

import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import * as electron from 'electron';
import * as releasesClient from '../../src/main/releases-client';
import { ReleasesApiError } from '../../src/main/releases-client';
import * as nfoGenerator from '../../src/main/nfo-generator';
import * as staging from '../../src/main/staging';
import * as archiver from '../../src/main/archiver';
import * as fsHelpers from '../../src/main/fs-helpers';

describe('registerIpcHandlers', () => {
  const cfg = { baseUrl: 'https://x', writeKey: 'k', isPlaceholderKey: false };

  beforeEach(() => {
    vi.clearAllMocks();
    (fsHelpers.pathExists as any).mockResolvedValue(false);
    // Default: the server could not reach the site, so no numbering warning.
    (releasesClient.fetchNumberingCheck as any).mockResolvedValue({ checked: false });
    registerIpcHandlers('/root', cfg);
  });

  it('loadSeedState maps the seed response fields into SeedState', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [{ nickname: 'spot', role: 'leader' }],
      nextReleaseNumber: 5,
      history: [{ releaseNumber: '4', filenames: 'x', title: 't', category: 'c', creators: 'cr' }],
      currentDizText: 'diz',
      revision: 9,
    });
    const result = await handlers.loadSeedState();
    expect(result).toEqual({
      members: [{ nickname: 'spot', role: 'leader' }],
      nextReleaseNumber: 5,
      history: [{ releaseNumber: '4', filenames: 'x', title: 't', category: 'c', creators: 'cr' }],
      currentDizText: 'diz',
      revision: 9,
    });
  });

  it('loadSeedState falls back to empty string when currentDizText is null', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: null, revision: 0,
    });
    const result = await handlers.loadSeedState();
    expect(result.currentDizText).toBe('');
  });

  it('buildRelease calls publishRelease BEFORE generateOutputs, passing the published number/history through', async () => {
    const callOrder: string[] = [];
    (releasesClient.publishRelease as any).mockImplementation(async () => {
      callOrder.push('publish');
      return { releaseNumber: 7, revision: 3, history: [{ releaseNumber: '7', filenames: 'up-x.lha', title: 't', category: 'c', creators: 'cr' }] };
    });
    (nfoGenerator.generateOutputs as any).mockImplementation(() => {
      callOrder.push('generate');
      return { nfoText: 'nfo', dizText: 'diz', releaseNumber: 7 };
    });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const form = { code: 'AAAAA', format: 'LHA' } as any;
    const result = await handlers.buildRelease(null, form, false);

    expect(callOrder).toEqual(['publish', 'generate']);
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(
      [{ releaseNumber: '7', filenames: 'up-x.lha', title: 't', category: 'c', creators: 'cr' }],
      7,
      form,
    );
    expect(result.releaseNumber).toBe(7);
    expect(result.archivePath).toContain('UP-AAAAA.LHA');
    expect(result.isDryRun).toBe(false);
    expect(releasesClient.fetchSeed).not.toHaveBeenCalled();
  });

  it('buildRelease with dryRun:true never calls publishRelease, uses fetchSeed instead, and synthesizes the preview history entry', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [],
      nextReleaseNumber: 8,
      history: [{ releaseNumber: '7', filenames: 'up-old.lha', title: 'old', category: 'Demo', creators: 'spot' }],
      currentDizText: '',
      revision: 3,
    });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 8 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const form = {
      code: 'BBBBB',
      format: 'ZIP',
      entryTitle: 'new thing',
      entryCategory: 'Music',
      entryCreators: 'spot, qwan',
    } as any;
    const result = await handlers.buildRelease(null, form, true);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
    expect(releasesClient.fetchSeed).toHaveBeenCalledWith(cfg);
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(
      [
        { releaseNumber: '8', filenames: 'up-bbbbb.zip', title: 'new thing', category: 'Music', creators: 'spot, qwan' },
        { releaseNumber: '7', filenames: 'up-old.lha', title: 'old', category: 'Demo', creators: 'spot' },
      ],
      8,
      form,
    );
    expect(result.releaseNumber).toBe(8);
    expect(result.isDryRun).toBe(true);
  });

  it('checkCodeAvailability delegates to checkCodeExists', async () => {
    (releasesClient.checkCodeExists as any).mockResolvedValue(true);
    const result = await handlers.checkCodeAvailability(null, 'AAAAA');
    expect(result).toEqual({ exists: true });
    expect(releasesClient.checkCodeExists).toHaveBeenCalledWith(cfg, 'AAAAA');
  });

  it('buildRelease throws locally and never calls publishRelease when UP-<CODE> already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA');

    const form = { code: 'AAAAA', format: 'LHA' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('buildRelease throws locally when a stale UP-<CODE>.LHA archive already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA.LHA');

    const form = { code: 'AAAAA', format: 'LHA' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('buildRelease throws locally when a stale UP-<CODE>.ZIP archive already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA.ZIP');

    const form = { code: 'AAAAA', format: 'ZIP' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('loadSeedState sets configWarning when running with the placeholder write key', async () => {
    vi.clearAllMocks();
    (fsHelpers.pathExists as any).mockResolvedValue(false);
    registerIpcHandlers('/root', { baseUrl: 'https://x', writeKey: 'REPLACE_ME_BEFORE_DISTRIBUTING', isPlaceholderKey: true });
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: '', revision: 0,
    });

    const result = await handlers.loadSeedState();

    expect(result.configWarning).toMatch(/RELEASES_WRITE_KEY/);
  });

  it('loadSeedState omits configWarning when a real write key is configured', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: '', revision: 0,
    });

    const result = await handlers.loadSeedState();

    expect(result.configWarning).toBeUndefined();
  });

  it('loadSeedState warns when the catalogue numbering has drifted from the site', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 217, history: [], currentDizText: '', revision: 0,
    });
    (releasesClient.fetchNumberingCheck as any).mockResolvedValue({
      checked: true,
      ok: false,
      summary: 'Release numbering disagrees with www.uprough.net: the site already has #217 "Hypocrisy".',
    });

    const result = await handlers.loadSeedState();

    expect(result.numberingWarning).toMatch(/#217/);
    expect(result.numberingWarning).toMatch(/NFO/);
  });

  it('loadSeedState omits the numbering warning when the catalogue and the site agree', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 218, history: [], currentDizText: '', revision: 0,
    });
    (releasesClient.fetchNumberingCheck as any).mockResolvedValue({ checked: true, ok: true, summary: 'agree' });

    const result = await handlers.loadSeedState();

    expect(result.numberingWarning).toBeUndefined();
  });

  it('loadSeedState still loads when the numbering check itself fails', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 3, history: [], currentDizText: '', revision: 0,
    });
    (releasesClient.fetchNumberingCheck as any).mockRejectedValue(new Error('releases.uprough.net unreachable'));

    const result = await handlers.loadSeedState();

    expect(result.nextReleaseNumber).toBe(3);
    expect(result.numberingWarning).toBeUndefined();
  });

  it('pickImage opens a file dialog restricted to image types and returns the chosen path', async () => {
    (electron.dialog.showOpenDialog as any).mockResolvedValue({ canceled: false, filePaths: ['/tmp/cover.png'] });
    const result = await handlers.pickImage();
    expect(result).toBe('/tmp/cover.png');
    const dialogArgs = (electron.dialog.showOpenDialog as any).mock.calls[0][0];
    expect(dialogArgs.filters).toEqual([{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'gif'] }]);
  });

  it('pickImage returns null when the dialog is canceled', async () => {
    (electron.dialog.showOpenDialog as any).mockResolvedValue({ canceled: true, filePaths: [] });
    const result = await handlers.pickImage();
    expect(result).toBeNull();
  });

  it('publishToWeb delegates to the releases-client function and returns an ok:true outcome', async () => {
    (releasesClient.publishToWeb as any).mockResolvedValue({ archiveUrl: 'a', wordpressPostUrl: 'b' });
    const result = await handlers.publishToWeb(null, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(result).toEqual({ ok: true, archiveUrl: 'a', wordpressPostUrl: 'b' });
    expect(releasesClient.publishToWeb).toHaveBeenCalledWith(cfg, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
  });

  it('publishToWeb returns an ok:false outcome for a plain (non-409) ReleasesApiError, not a thrown Error', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(new ReleasesApiError('boom', { status: 500 }));
    const result = await handlers.publishToWeb(null, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(result).toEqual({ ok: false, message: 'boom', status: 500, wordpressPostUrl: undefined });
  });

  it('publishToWeb returns an ok:false outcome carrying status and wordpressPostUrl on a 409, ' +
    'rather than throwing — ipcMain.handle mangles a thrown Error message crossing the IPC boundary, ' +
    'but a plain returned object crosses intact', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(
      new ReleasesApiError('already published', { status: 409, wordpressPostUrl: 'https://www.uprough.net/x/' }),
    );

    const result = await handlers.publishToWeb(null, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);

    expect(result).toEqual({
      ok: false,
      message: 'already published',
      status: 409,
      wordpressPostUrl: 'https://www.uprough.net/x/',
    });
  });

  it('publishToWeb returns an ok:false outcome with just a message for a non-ReleasesApiError failure', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(new Error('disk full'));
    const result = await handlers.publishToWeb(null, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(result).toEqual({ ok: false, message: 'disk full' });
  });

  it('deleteRelease delegates to the releases-client function and returns an ok:true outcome', async () => {
    (releasesClient.deleteRelease as any).mockResolvedValue({
      deleted: true,
      code: 'AAAAA',
      releaseNumber: 7,
      wasPublished: false,
      wordpressPostUrl: null,
      archiveRemoved: false,
    });
    const result = await handlers.deleteRelease(null, 'AAAAA');
    expect(result).toEqual({
      ok: true,
      code: 'AAAAA',
      releaseNumber: 7,
      wasPublished: false,
      wordpressPostUrl: null,
      archiveRemoved: false,
    });
    expect(releasesClient.deleteRelease).toHaveBeenCalledWith(cfg, 'AAAAA');
  });

  it('deleteRelease returns an ok:false outcome carrying status for a 409 (not the latest release), not a thrown Error', async () => {
    (releasesClient.deleteRelease as any).mockRejectedValue(new ReleasesApiError('not the latest', { status: 409 }));
    const result = await handlers.deleteRelease(null, 'AAAAA');
    expect(result).toEqual({ ok: false, message: 'not the latest', status: 409 });
  });

  it('deleteRelease returns an ok:false outcome for a non-ReleasesApiError failure', async () => {
    (releasesClient.deleteRelease as any).mockRejectedValue(new Error('network down'));
    const result = await handlers.deleteRelease(null, 'AAAAA');
    expect(result).toEqual({ ok: false, message: 'network down' });
  });
});
