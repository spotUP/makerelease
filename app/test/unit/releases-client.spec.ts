import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs';
import { fetchSeed, checkCodeExists, publishRelease, publishToWeb, ReleasesApiError } from '../../src/main/releases-client';
import { ReleaseFormData } from '../../src/shared/types';

const cfg = { baseUrl: 'https://releases.example.test', writeKey: 'secret', isPlaceholderKey: false };

const sampleForm: ReleaseFormData = {
  code: 'TEST0',
  format: 'LHA',
  releaseName: "'TEST'",
  platformTag: 'A1200',
  releaseNotesText: 'notes',
  newsText: 'news',
  dizBannerText: 'diz',
  members: [{ nickname: 'spot', role: 'leader' }],
  entryTitle: 'test',
  entryCategory: 'Demo',
  entryCreators: 'spot',
  selectedPaths: ['/tmp/a.exe'],
};

describe('fetchSeed', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('GETs /api/releases/seed and returns the parsed JSON', async () => {
    const body = { members: [], nextReleaseNumber: 1, history: [], currentDizText: null, revision: 0 };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    const result = await fetchSeed(cfg);

    expect(global.fetch).toHaveBeenCalledWith('https://releases.example.test/api/releases/seed', expect.any(Object));
    expect(result).toEqual(body);
  });

  it('throws ReleasesApiError on a non-2xx response', async () => {
    (global.fetch as any).mockResolvedValue({ ok: false, status: 500, json: async () => ({ error: 'boom' }) });
    await expect(fetchSeed(cfg)).rejects.toThrow(ReleasesApiError);
  });

  it('throws ReleasesApiError when fetch itself rejects (network failure)', async () => {
    (global.fetch as any).mockRejectedValue(new Error('ECONNREFUSED'));
    await expect(fetchSeed(cfg)).rejects.toThrow(ReleasesApiError);
  });
});

describe('checkCodeExists', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('GETs /api/releases/:code/exists and returns the boolean', async () => {
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => ({ exists: true }) });
    const result = await checkCodeExists(cfg, 'TEST0');
    expect(global.fetch).toHaveBeenCalledWith(
      'https://releases.example.test/api/releases/TEST0/exists',
      expect.any(Object),
    );
    expect(result).toBe(true);
  });
});

describe('publishRelease', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('POSTs the mapped body with the write key header and returns the parsed response', async () => {
    const body = { releaseNumber: 1, revision: 1, history: [] };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    const result = await publishRelease(cfg, sampleForm);

    expect(global.fetch).toHaveBeenCalledTimes(1);
    const [url, init] = (global.fetch as any).mock.calls[0];
    expect(url).toBe('https://releases.example.test/api/releases');
    expect(init.method).toBe('POST');
    expect(init.headers['X-Release-Key']).toBe('secret');
    const sentBody = JSON.parse(init.body);
    expect(sentBody).toEqual({
      code: 'TEST0',
      format: 'LHA',
      releaseName: "'TEST'",
      platformTag: 'A1200',
      releaseNotes: 'notes',
      news: 'news',
      dizText: 'diz',
      entryTitle: 'test',
      entryCategory: 'Demo',
      entryCreators: 'spot',
      members: [{ nickname: 'spot', role: 'leader' }],
    });
    expect(result).toEqual(body);
  });

  it('throws ReleasesApiError with the server error message on a 400', async () => {
    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 400,
      json: async () => ({ error: 'code AAAAA already exists' }),
    });
    await expect(publishRelease(cfg, sampleForm)).rejects.toThrow('code AAAAA already exists');
  });
});

describe('publishToWeb', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('POSTs a multipart form with the archive and image file contents, and the write key header', async () => {
    const archivePath = '/tmp/mr-test-archive.lha';
    const imagePath = '/tmp/mr-test-image.png';
    fs.writeFileSync(archivePath, 'fake archive bytes');
    fs.writeFileSync(imagePath, 'fake image bytes');

    const body = { archiveUrl: 'https://www.uprough.net/releases/UP-AAAAA.LHA', wordpressPostUrl: 'https://www.uprough.net/x/' };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    const result = await publishToWeb(cfg, 'AAAAA', archivePath, imagePath);

    expect(result).toEqual(body);
    const [url, init] = (global.fetch as any).mock.calls[0];
    expect(url).toBe('https://releases.example.test/api/releases/AAAAA/publish');
    expect(init.method).toBe('POST');
    expect(init.headers['X-Release-Key']).toBe('secret');
    expect(init.body).toBeInstanceOf(FormData);

    fs.unlinkSync(archivePath);
    fs.unlinkSync(imagePath);
  });

  it('throws ReleasesApiError with the server message on failure', async () => {
    fs.writeFileSync('/tmp/mr-test-archive2.lha', 'a');
    fs.writeFileSync('/tmp/mr-test-image2.png', 'i');
    (global.fetch as any).mockResolvedValue({ ok: false, status: 409, json: async () => ({ error: 'already published' }) });
    await expect(publishToWeb(cfg, 'AAAAA', '/tmp/mr-test-archive2.lha', '/tmp/mr-test-image2.png')).rejects.toThrow(
      'already published',
    );
    fs.unlinkSync('/tmp/mr-test-archive2.lha');
    fs.unlinkSync('/tmp/mr-test-image2.png');
  });
});
