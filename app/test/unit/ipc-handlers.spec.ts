import { describe, it, expect, vi, beforeEach } from 'vitest';

const handlers: Record<string, (...args: any[]) => any> = {};

vi.mock('electron', () => ({
  ipcMain: { handle: vi.fn((channel: string, fn: any) => { handlers[channel] = fn; }) },
  dialog: { showOpenDialog: vi.fn() },
  shell: { openPath: vi.fn(), trashItem: vi.fn() },
}));

// Profiles come from parseProfile on fixed JSON, so every handler sees exactly
// what a real profile.json would give it; only the disk read is skipped (the
// real one is covered by profiles.spec.ts and reach-profiles.spec.ts).
const TEMPLATES = { nfo: "{{group}}\n", diz: "{{code}}\n" };
const PROFILE_JSON: Record<string, unknown> = {
  uprough: {
    label: 'Up Rough', url: 'https://uprough.net', filePrefix: 'UP-', renderer: 'uprough',
    backend: 'releases-doorserver', server: { baseUrl: 'https://x', writeKey: 'k' },
  },
  xinnovations: {
    label: 'Up Rough /X Innovations', url: 'https://uprough.net', filePrefix: 'UP-', renderer: 'xinnovations',
    backend: 'releases-doorserver', server: { baseUrl: 'https://x', writeKey: 'k' },
  },
  nokey: {
    label: 'No Key', url: 'https://uprough.net', filePrefix: 'UP-', renderer: 'uprough',
    backend: 'releases-doorserver', server: { baseUrl: 'https://x' },
  },
  example: {
    label: 'Example Group', url: 'https://example.net', filePrefix: 'EX-', renderer: 'placeholder',
    members: [{ nickname: 'coder', role: 'code' }],
  },
};

vi.mock('../../src/main/profiles', async () => {
  const { parseProfile } = await import('../../src/shared/profile');
  return {
    loadProfile: vi.fn(async (_dir: string, id: string) => {
      if (!(id in PROFILE_JSON)) throw new Error(`no profile ${id}`);
      return parseProfile(PROFILE_JSON[id], id, `${id}/profile.json`);
    }),
    listProfiles: vi.fn(),
    saveProfile: vi.fn(),
    deleteProfile: vi.fn(),
    readTemplateFile: vi.fn(),
    readTemplates: vi.fn(async () => TEMPLATES),
    isProfileLoadError: (entry: { error?: string }) => typeof entry.error === 'string',
  };
});

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
    fetchManifest: vi.fn(),
    fetchNumberingCheck: vi.fn(),
    checkCodeExists: vi.fn(),
    publishRelease: vi.fn(),
    deleteRelease: vi.fn(),
    publishToWeb: vi.fn(),
    clientConfigFor: (profile: { server?: { baseUrl: string; writeKey: string } }) => ({
      baseUrl: profile.server!.baseUrl,
      writeKey: profile.server!.writeKey,
    }),
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

vi.mock('../../src/main/door-repo-client', () => ({
  submitDoor: vi.fn(),
}));

vi.mock('../../src/main/aminet', () => ({
  uploadToAminet: vi.fn(),
}));

vi.mock('../../src/main/fs-helpers', () => ({
  pathExists: vi.fn(),
  ensureDir: vi.fn(),
  removePath: vi.fn(),
}));

import { registerIpcHandlers } from '../../src/main/ipc-handlers';
import * as electron from 'electron';
import * as profilesModule from '../../src/main/profiles';
import * as releasesClient from '../../src/main/releases-client';
import { ReleasesApiError } from '../../src/main/releases-client';
import * as nfoGenerator from '../../src/main/nfo-generator';
import * as staging from '../../src/main/staging';
import * as archiver from '../../src/main/archiver';
import * as fsHelpers from '../../src/main/fs-helpers';
import * as doorRepo from '../../src/main/door-repo-client';
import * as aminet from '../../src/main/aminet';

describe('registerIpcHandlers', () => {
  const cfg = { baseUrl: 'https://x', writeKey: 'k' };
  const outputRootFor = () => '/root';
  const UR = expect.objectContaining({ id: 'uprough', filePrefix: 'UP-' });
  const SEED_BASE = {
    profile: {
      id: 'uprough', label: 'Up Rough', url: 'https://uprough.net', filePrefix: 'UP-',
      codeLength: 5, renderer: 'uprough', backend: 'releases-doorserver',
    },
    outputRoot: '/root',
  };

  beforeEach(() => {
    vi.clearAllMocks();
    (fsHelpers.pathExists as any).mockResolvedValue(false);
    (fsHelpers.ensureDir as any).mockResolvedValue(undefined);
    (fsHelpers.removePath as any).mockResolvedValue(undefined);
    (doorRepo.submitDoor as any).mockResolvedValue({ ok: true, id: 12, message: 'in the queue' });
    // Default: the server could not reach the site, so no numbering warning.
    (releasesClient.fetchNumberingCheck as any).mockResolvedValue({ checked: false });
    registerIpcHandlers('/profiles', outputRootFor);
  });

  it('loadSeedState maps the seed response fields into SeedState', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [{ nickname: 'spot', role: 'leader' }],
      nextReleaseNumber: 5,
      history: [{ releaseNumber: '4', filenames: 'x', title: 't', category: 'c', creators: 'cr' }],
      currentDizText: 'diz',
      revision: 9,
    });
    const result = await handlers.loadSeedState(null, 'uprough');
    expect(result).toEqual({
      ...SEED_BASE,
      members: [{ nickname: 'spot', role: 'leader' }],
      boards: [],
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
    const result = await handlers.loadSeedState(null, 'uprough');
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

    const form = { profileId: 'uprough', code: 'AAAAA', format: 'LHA' } as any;
    const result = await handlers.buildRelease(null, form, false);

    expect(callOrder).toEqual(['publish', 'generate']);
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(
      UR,
      [{ releaseNumber: '7', filenames: 'up-x.lha', title: 't', category: 'c', creators: 'cr' }],
      7,
      { ...form, division: 'uprough' },
      [],
      undefined, // code renderers read no template files
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
      profileId: 'uprough',
      code: 'BBBBB',
      format: 'ZIP',
      entryTitle: 'new thing',
      entryCategory: 'Music',
      entryCreators: 'spot, qwan',
    } as any;
    const result = await handlers.buildRelease(null, form, true);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
    expect(releasesClient.fetchSeed).toHaveBeenCalledWith(cfg, 'uprough');
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(
      UR,
      [
        { releaseNumber: '8', filenames: 'up-bbbbb.zip', title: 'new thing', category: 'Music', creators: 'spot, qwan' },
        { releaseNumber: '7', filenames: 'up-old.lha', title: 'old', category: 'Demo', creators: 'spot' },
      ],
      8,
      { ...form, division: 'uprough' },
      [],
      undefined, // code renderers read no template files
    );
    expect(result.releaseNumber).toBe(8);
    expect(result.isDryRun).toBe(true);
  });

  it('checkCodeAvailability delegates to checkCodeExists', async () => {
    (releasesClient.checkCodeExists as any).mockResolvedValue(true);
    const result = await handlers.checkCodeAvailability(null, 'uprough', 'AAAAA');
    expect(result).toEqual({ exists: true });
    expect(releasesClient.checkCodeExists).toHaveBeenCalledWith(cfg, 'AAAAA');
  });

  it('buildRelease throws locally and never calls publishRelease when UP-<CODE> already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA');

    const form = { profileId: 'uprough', code: 'AAAAA', format: 'LHA' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('buildRelease throws locally when a stale UP-<CODE>.LHA archive already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA.LHA');

    const form = { profileId: 'uprough', code: 'AAAAA', format: 'LHA' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('buildRelease throws locally when a stale UP-<CODE>.ZIP archive already exists on disk', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-AAAAA.ZIP');

    const form = { profileId: 'uprough', code: 'AAAAA', format: 'ZIP' } as any;
    await expect(handlers.buildRelease(null, form)).rejects.toThrow(/UP-AAAAA already exists locally/);

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('loadSeedState sets configWarning when the profile has no write key', async () => {
    vi.clearAllMocks();
    (fsHelpers.pathExists as any).mockResolvedValue(false);
    (fsHelpers.ensureDir as any).mockResolvedValue(undefined);
    (fsHelpers.removePath as any).mockResolvedValue(undefined);
    (doorRepo.submitDoor as any).mockResolvedValue({ ok: true, id: 12, message: 'in the queue' });
    registerIpcHandlers('/profiles', outputRootFor);
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: '', revision: 0,
    });

    const result = await handlers.loadSeedState(null, 'nokey');

    expect(result.configWarning).toMatch(/"nokey" profile has no server write key/);
  });

  it('loadSeedState omits configWarning when a real write key is configured', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: '', revision: 0,
    });

    const result = await handlers.loadSeedState(null, 'uprough');

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

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.numberingWarning).toMatch(/#217/);
    expect(result.numberingWarning).toMatch(/NFO/);
  });

  it('loadSeedState omits the numbering warning when the catalogue and the site agree', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 218, history: [], currentDizText: '', revision: 0,
    });
    (releasesClient.fetchNumberingCheck as any).mockResolvedValue({ checked: true, ok: true, summary: 'agree' });

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.numberingWarning).toBeUndefined();
  });

  it('loadSeedState still loads when the numbering check itself fails', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 3, history: [], currentDizText: '', revision: 0,
    });
    (releasesClient.fetchNumberingCheck as any).mockRejectedValue(new Error('releases.uprough.net unreachable'));

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.nextReleaseNumber).toBe(3);
    expect(result.numberingWarning).toBeUndefined();
  });

  it('loadSeedState warns about a build that was never published or revoked', async () => {
    const twoDaysAgo = Math.floor(Date.now() / 1000) - 2 * 24 * 60 * 60;
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 219, history: [], currentDizText: '', revision: 0,
      drafts: [{ code: 'EVK26', releaseNumber: 217, createdAt: twoDaysAgo, entryTitle: 'evoke 2026' }],
    });

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.staleDraftWarning).toMatch(/EVK26/);
    expect(result.staleDraftWarning).toMatch(/release 217/);
  });

  it('loadSeedState says nothing about a draft made minutes ago', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 219, history: [], currentDizText: '', revision: 0,
      drafts: [{ code: 'EVK26', releaseNumber: 218, createdAt: Math.floor(Date.now() / 1000) - 120, entryTitle: 'x' }],
    });

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.staleDraftWarning).toBeUndefined();
  });

  it('loadSeedState copes with a server that does not report drafts at all', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 1, history: [], currentDizText: '', revision: 0,
    });

    const result = await handlers.loadSeedState(null, 'uprough');

    expect(result.staleDraftWarning).toBeUndefined();
  });

  it('a dry run writes into its own folder, not where a real build lands', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 9, history: [], currentDizText: '', revision: 1,
    });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 9 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const result = await handlers.buildRelease(null, { profileId: 'uprough', code: 'MINDR', format: 'LHA' } as any, true);

    expect(result.archivePath).toBe('/root/dry-run/UP-MINDR.LHA');
    expect(archiver.packArchive).toHaveBeenCalledWith(UR, '/tmp/outer', 'MINDR', 'LHA', '/root/dry-run');
    expect(archiver.finalizeRelease).toHaveBeenCalledWith(UR, '/tmp/outer', 'MINDR', '/root/dry-run');
    expect(fsHelpers.ensureDir).toHaveBeenCalledWith('/root/dry-run');
  });

  it('builds for real straight after a dry run of the same code', async () => {
    // The reported failure: the dry run left UP-MINDR where a real build
    // lands, and assertLocalCodeAvailable then refused the real build with
    // "already exists locally ... leftover from a previous attempt".
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p.includes('/dry-run/'));
    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 9, revision: 2, history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 9 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const result = await handlers.buildRelease(null, { profileId: 'uprough', code: 'MINDR', format: 'LHA' } as any, false);

    expect(result.archivePath).toBe('/root/UP-MINDR.LHA');
    expect(archiver.packArchive).toHaveBeenCalledWith(UR, '/tmp/outer', 'MINDR', 'LHA', '/root');
  });

  it('a second dry run replaces the previous preview instead of refusing it', async () => {
    (fsHelpers.pathExists as any).mockResolvedValue(true);
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 9, history: [], currentDizText: '', revision: 1,
    });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 9 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    await expect(handlers.buildRelease(null, { profileId: 'uprough', code: 'MINDR', format: 'LHA' } as any, true)).resolves.toBeTruthy();

    expect(fsHelpers.removePath).toHaveBeenCalledWith('/root/dry-run/UP-MINDR');
    expect(fsHelpers.removePath).toHaveBeenCalledWith('/root/dry-run/UP-MINDR.LHA');
  });

  it('still refuses a real build when a real leftover is in the way', async () => {
    (fsHelpers.pathExists as any).mockImplementation(async (p: string) => p === '/root/UP-MINDR');

    await expect(handlers.buildRelease(null, { profileId: 'uprough', code: 'MINDR', format: 'LHA' } as any, false)).rejects.toThrow(
      /already exists locally/,
    );
    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
  });

  it('builds a /X release with a list of every release as its own art, itself first', async () => {
    const X_BANNER = [
      '    _ _ __ ___    ____  _ _ __ _________',
      '   | | Y  / _ \\../ __ \\| | Y  / __  | | |',
      'dNo| | | | | | || /_ | | | | | |_ | l | |dS!',
      '|  | | | | |`| || || | | | | | || |___| |  |',
      "|  | | l | |__/'| || | | | | | || |   | |  |",
      '|  | l___| |    | || | | l l | ll | | | |  |',
      "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |",
      '|                                          |',
      '|     uP rOUGH /X iNNOVATiONS pRESENTS:    |',
      '|              tHE nEW oNE v1.0            |',
      '|             a tAGLINE hERE!              |',
      "`------------------------------------------'",
    ].join('\n');

    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 8, revision: 2, history: [] });
    (releasesClient.fetchManifest as any).mockResolvedValue({
      history: [{ releaseNumber: 'oo7', filenames: 'up-md15.lha', title: 'more doors', dizText: X_BANNER }],
    });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 8 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const form = {
      profileId: 'xinnovations',
      code: 'XNEWW',
      format: 'LHA',
      entryTitle: 'the new one',
      dizBannerText: X_BANNER,
    } as any;
    await handlers.buildRelease(null, form, false);

    const releaseList = (nfoGenerator.generateOutputs as any).mock.calls[0][4];
    expect(releaseList[0].displayNumber).toBe('oo8');
    expect(releaseList[1].displayNumber).toBe('oo7');
    // Name kept, tagline dropped - every entry in the list has the same shape.
    expect(releaseList[0].art).toContain('tHE nEW oNE v1.0');
    expect(releaseList[0].art).not.toContain('a tAGLINE hERE!');
  });

  it('an Up Rough build never asks for the manifest', async () => {
    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 9, revision: 2, history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 9 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    await handlers.buildRelease(null, { profileId: 'uprough', code: 'UPNEW', format: 'LHA' } as any, false);

    expect(releasesClient.fetchManifest).not.toHaveBeenCalled();
  });

  it('sends a /X release to the door repository and reports what it said', async () => {
    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 8, revision: 2, history: [] });
    (releasesClient.fetchManifest as any).mockResolvedValue({ history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 8 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const result = await handlers.buildRelease(
      null,
      { profileId: 'xinnovations', code: 'XNEWW', format: 'LHA', dizBannerText: '' } as any,
      false,
    );

    expect(doorRepo.submitDoor).toHaveBeenCalledWith('/root/UP-XNEWW.LHA');
    expect(result.doorSubmission).toEqual({ ok: true, id: 12, message: 'in the queue' });
  });

  it('does not put a dry run in anybody queue', async () => {
    (releasesClient.fetchSeed as any).mockResolvedValue({
      members: [], nextReleaseNumber: 8, history: [], currentDizText: '', revision: 1,
    });
    (releasesClient.fetchManifest as any).mockResolvedValue({ history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 8 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const result = await handlers.buildRelease(
      null,
      { profileId: 'xinnovations', code: 'XNEWW', format: 'LHA', dizBannerText: '' } as any,
      true,
    );

    expect(doorRepo.submitDoor).not.toHaveBeenCalled();
    expect(result.doorSubmission).toBeUndefined();
  });

  it('never sends an Up Rough release to the door repository - a demo is not a door', async () => {
    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 9, revision: 2, history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 9 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    await handlers.buildRelease(null, { profileId: 'uprough', code: 'UPNEW', format: 'LHA' } as any, false);

    expect(doorRepo.submitDoor).not.toHaveBeenCalled();
  });

  it('still returns a finished build when the door repository refuses it', async () => {
    // The archive exists and the release is already in the catalogue; a
    // refusal from a separate service does not undo either.
    (doorRepo.submitDoor as any).mockResolvedValue({ ok: false, message: 'the repository already has that archive' });
    (releasesClient.publishRelease as any).mockResolvedValue({ releaseNumber: 8, revision: 2, history: [] });
    (releasesClient.fetchManifest as any).mockResolvedValue({ history: [] });
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 8 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);

    const result = await handlers.buildRelease(
      null,
      { profileId: 'xinnovations', code: 'XNEWW', format: 'LHA', dizBannerText: '' } as any,
      false,
    );

    expect(result.archivePath).toBe('/root/UP-XNEWW.LHA');
    expect(result.doorSubmission?.ok).toBe(false);
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
    const result = await handlers.publishToWeb(null, 'uprough', 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(result).toEqual({ ok: true, archiveUrl: 'a', wordpressPostUrl: 'b' });
    expect(releasesClient.publishToWeb).toHaveBeenCalledWith(cfg, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
  });

  it('uploadToAminet hands the request and archive to the Aminet uploader and returns its outcome', async () => {
    (aminet.uploadToAminet as any).mockResolvedValue({ ok: false, message: 'curl missing', readmePath: '/tmp/archive.readme' });
    const request = { readmeText: 'Short: x\n', uploaderEmail: 'me@example.com' };
    const result = await handlers.uploadToAminet(null, request, '/tmp/archive.lha');
    expect(result).toEqual({ ok: false, message: 'curl missing', readmePath: '/tmp/archive.readme' });
    expect(aminet.uploadToAminet).toHaveBeenCalledWith(request, '/tmp/archive.lha');
  });

  it('publishToWeb never uploads to Aminet itself; the window does that as its own step', async () => {
    (releasesClient.publishToWeb as any).mockResolvedValue({ archiveUrl: 'a', wordpressPostUrl: 'b' });
    await handlers.publishToWeb(null, 'uprough', 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(aminet.uploadToAminet).not.toHaveBeenCalled();
  });

  it('publishToWeb returns an ok:false outcome for a plain (non-409) ReleasesApiError, not a thrown Error', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(new ReleasesApiError('boom', { status: 500 }));
    const result = await handlers.publishToWeb(null, 'uprough', 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
    expect(result).toEqual({ ok: false, message: 'boom', status: 500, wordpressPostUrl: undefined });
  });

  it('publishToWeb returns an ok:false outcome carrying status and wordpressPostUrl on a 409, ' +
    'rather than throwing — ipcMain.handle mangles a thrown Error message crossing the IPC boundary, ' +
    'but a plain returned object crosses intact', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(
      new ReleasesApiError('already published', { status: 409, wordpressPostUrl: 'https://www.uprough.net/x/' }),
    );

    const result = await handlers.publishToWeb(null, 'uprough', 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);

    expect(result).toEqual({
      ok: false,
      message: 'already published',
      status: 409,
      wordpressPostUrl: 'https://www.uprough.net/x/',
    });
  });

  it('publishToWeb returns an ok:false outcome with just a message for a non-ReleasesApiError failure', async () => {
    (releasesClient.publishToWeb as any).mockRejectedValue(new Error('disk full'));
    const result = await handlers.publishToWeb(null, 'uprough', 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png', []);
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
    const result = await handlers.deleteRelease(null, 'uprough', 'AAAAA');
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
    const result = await handlers.deleteRelease(null, 'uprough', 'AAAAA');
    expect(result).toEqual({ ok: false, message: 'not the latest', status: 409 });
  });

  it('deleteRelease returns an ok:false outcome for a non-ReleasesApiError failure', async () => {
    (releasesClient.deleteRelease as any).mockRejectedValue(new Error('network down'));
    const result = await handlers.deleteRelease(null, 'uprough', 'AAAAA');
    expect(result).toEqual({ ok: false, message: 'network down' });
  });

  it('builds a release for a server-less profile under its own prefix, without touching the server', async () => {
    (nfoGenerator.generateOutputs as any).mockReturnValue({ nfoText: 'nfo', dizText: 'diz', releaseNumber: 0 });
    (staging.stageRelease as any).mockResolvedValue('/tmp/outer');
    (archiver.packArchive as any).mockResolvedValue(undefined);
    (archiver.finalizeRelease as any).mockResolvedValue(undefined);
    const EX = expect.objectContaining({ id: 'example', filePrefix: 'EX-', backend: 'none' });

    const result = await handlers.buildRelease(
      null,
      { profileId: 'example', code: 'DEMO1', format: 'LHA', division: 'xinnovations' } as any,
      false,
    );

    expect(releasesClient.publishRelease).not.toHaveBeenCalled();
    expect(releasesClient.fetchSeed).not.toHaveBeenCalled();
    expect(fsHelpers.pathExists).toHaveBeenCalledWith('/root/EX-DEMO1');
    // A placeholder profile has no division, so whatever the window sent is dropped.
    const sentForm = (nfoGenerator.generateOutputs as any).mock.calls[0][3];
    expect(sentForm.division).toBeUndefined();
    expect(nfoGenerator.generateOutputs).toHaveBeenCalledWith(EX, [], 0, sentForm, [], TEMPLATES);
    expect(archiver.packArchive).toHaveBeenCalledWith(EX, '/tmp/outer', 'DEMO1', 'LHA', '/root');
    expect(result.archivePath).toBe('/root/EX-DEMO1.LHA');
  });

  it('loadSeedState for a server-less profile seeds from the profile members and never calls the server', async () => {
    const result = await handlers.loadSeedState(null, 'example');
    expect(releasesClient.fetchSeed).not.toHaveBeenCalled();
    expect(result.profile).toMatchObject({ id: 'example', filePrefix: 'EX-', backend: 'none' });
    expect(result.members).toEqual([{ nickname: 'coder', role: 'code' }]);
    expect(result.nextReleaseNumber).toBe(0);
    expect(result.configWarning).toBeUndefined();
  });

  it('checkCodeAvailability for a server-less profile never asks the server', async () => {
    expect(await handlers.checkCodeAvailability(null, 'example', 'DEMO1')).toEqual({ exists: false });
    expect(releasesClient.checkCodeExists).not.toHaveBeenCalled();
  });

  it('listProfiles summarises loaded profiles and passes load errors through', async () => {
    const { listProfiles } = await import('../../src/main/profiles');
    const { parseProfile } = await import('../../src/shared/profile');
    (listProfiles as any).mockResolvedValue([
      parseProfile(PROFILE_JSON.example, 'example', 'example/profile.json'),
      { id: 'broken', error: 'broken/profile.json: filePrefix is missing' },
    ]);

    const result = await handlers.listProfiles(null);

    expect(listProfiles).toHaveBeenCalledWith('/profiles');
    expect(result).toEqual([
      { id: 'example', label: 'Example Group', url: 'https://example.net', filePrefix: 'EX-', codeLength: 5, renderer: 'placeholder', backend: 'none' },
      { id: 'broken', error: 'broken/profile.json: filePrefix is missing' },
    ]);
  });

  it('openProfilesFolder creates the folder and opens it', async () => {
    (electron.shell.openPath as any).mockResolvedValue('');
    await handlers.openProfilesFolder(null);
    expect(fsHelpers.ensureDir).toHaveBeenCalledWith('/profiles');
    expect(electron.shell.openPath).toHaveBeenCalledWith('/profiles');
  });

  it('openProfilesFolder reports a folder the system could not open', async () => {
    (electron.shell.openPath as any).mockResolvedValue('no handler');
    await expect(handlers.openProfilesFolder(null)).rejects.toThrow(/Could not open \/profiles: no handler/);
  });

  it('the profile editor channels delegate to the profiles module and return its outcome', async () => {
    const outcome = { ok: false, error: 'profile.json: "url" is missing' };
    (profilesModule.saveProfile as any).mockResolvedValue(outcome);
    (profilesModule.deleteProfile as any).mockImplementation(async (_dir: string, _id: string, trash: (p: string) => Promise<void>) => {
      await trash('/profiles/example');
      return { ok: true };
    });
    const request = { id: 'demo', isNew: true, profile: PROFILE_JSON.example };
    expect(await handlers.saveProfile(null, request)).toEqual(outcome);
    expect(profilesModule.saveProfile).toHaveBeenCalledWith('/profiles', request);
    expect(await handlers.deleteProfile(null, 'example')).toEqual({ ok: true });
    expect(electron.shell.trashItem).toHaveBeenCalledWith('/profiles/example');
  });

  it('loadProfileForEdit gives templates for a placeholder profile only; newProfileDraft starts as a placeholder group', async () => {
    const example = await handlers.loadProfileForEdit(null, 'example');
    expect(example.profile.id).toBe('example');
    expect(example.templates).toEqual(TEMPLATES);
    expect((await handlers.loadProfileForEdit(null, 'uprough')).templates).toEqual({ nfo: null, diz: '' });

    (profilesModule.listProfiles as any).mockResolvedValue([{ id: 'new-group' }]);
    const draft = await handlers.newProfileDraft(null);
    expect(draft.profile).toMatchObject({ id: 'new-group-2', renderer: 'placeholder', backend: 'none', codeLength: 5 });
    expect(draft.templates.diz).toContain('{{group');
  });

  it('importTemplate returns the decoded file and its name, or null when cancelled', async () => {
    (electron.dialog.showOpenDialog as any).mockResolvedValueOnce({ canceled: true, filePaths: [] });
    expect(await handlers.importTemplate(null)).toBeNull();
    (electron.dialog.showOpenDialog as any).mockResolvedValueOnce({ canceled: false, filePaths: ['/x/art.diz'] });
    (profilesModule.readTemplateFile as any).mockResolvedValue('ART\n');
    expect(await handlers.importTemplate(null)).toEqual({ text: 'ART\n', name: 'art.diz' });
    expect(profilesModule.readTemplateFile).toHaveBeenCalledWith('/x/art.diz', false);
  });
});
