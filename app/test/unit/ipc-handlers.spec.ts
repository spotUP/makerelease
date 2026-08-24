import { describe, it, expect, vi, beforeEach } from 'vitest';

const handlers: Record<string, (...args: any[]) => any> = {};

vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
}));

vi.mock('../../src/main/releases-client', () => ({
  fetchSeed: vi.fn(),
  checkCodeExists: vi.fn(),
  publishRelease: vi.fn(),
}));

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
import * as releasesClient from '../../src/main/releases-client';
import * as nfoGenerator from '../../src/main/nfo-generator';
import * as staging from '../../src/main/staging';
import * as archiver from '../../src/main/archiver';
import * as fsHelpers from '../../src/main/fs-helpers';

describe('registerIpcHandlers', () => {
  const cfg = { baseUrl: 'https://x', writeKey: 'k', isPlaceholderKey: false };

  beforeEach(() => {
    vi.clearAllMocks();
    (fsHelpers.pathExists as any).mockResolvedValue(false);
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
    const result = await handlers.buildRelease(null, form);

    expect(callOrder).toEqual(['publish', 'generate']);
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(
      [{ releaseNumber: '7', filenames: 'up-x.lha', title: 't', category: 'c', creators: 'cr' }],
      7,
      form,
    );
    expect(result.releaseNumber).toBe(7);
    expect(result.archivePath).toContain('UP-AAAAA.LHA');
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
});
