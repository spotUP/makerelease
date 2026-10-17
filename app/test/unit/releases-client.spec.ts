import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs';
import { clientConfigFor, fetchSeed, checkCodeExists, publishRelease, deleteRelease, publishToWeb, ReleasesApiError } from '../../src/main/releases-client';
import { ReleaseFormData } from '../../src/shared/types';

const cfg = { baseUrl: 'https://releases.example.test', writeKey: 'secret', isPlaceholderKey: false };

const sampleForm: ReleaseFormData = {
  profileId: 'uprough',
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

  it('asks for the division it was given', async () => {
    (global.fetch as any).mockResolvedValue({ ok: true, json: async () => ({ members: [] }) });
    await fetchSeed(cfg, 'xinnovations');
    expect(global.fetch).toHaveBeenCalledWith(
      'https://releases.example.test/api/releases/seed?division=xinnovations',
      expect.any(Object),
    );
  });

  it('GETs /api/releases/seed and returns the parsed JSON', async () => {
    const body = { members: [], nextReleaseNumber: 1, history: [], currentDizText: null, revision: 0 };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    const result = await fetchSeed(cfg);

    expect(global.fetch).toHaveBeenCalledWith(
      'https://releases.example.test/api/releases/seed?division=uprough',
      expect.any(Object),
    );
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

describe('deleteRelease', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('DELETEs /api/releases/:code with the write key header and returns the parsed JSON', async () => {
    const body = {
      deleted: true,
      code: 'TEST0',
      releaseNumber: 5,
      wasPublished: false,
      wasDraft: true,
      wordpressPostUrl: null,
      archiveRemoved: false,
      wordpressPostTrashed: false,
      wordpressPostError: null,
    };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    const result = await deleteRelease(cfg, 'TEST0');

    expect(global.fetch).toHaveBeenCalledWith('https://releases.example.test/api/releases/TEST0', {
      method: 'DELETE',
      headers: { 'X-Release-Key': 'secret' },
    });
    expect(result).toEqual(body);
  });

  it('throws ReleasesApiError with status on a 409 (not the latest release)', async () => {
    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 409,
      json: async () => ({ error: 'not the latest release' }),
    });
    await expect(deleteRelease(cfg, 'TEST0')).rejects.toThrow(ReleasesApiError);
    try {
      await deleteRelease(cfg, 'TEST0');
    } catch (err) {
      expect((err as ReleasesApiError).status).toBe(409);
    }
  });

  it('throws ReleasesApiError with status on a 404 (unknown code)', async () => {
    (global.fetch as any).mockResolvedValue({ ok: false, status: 404, json: async () => ({ error: 'no release found' }) });
    await expect(deleteRelease(cfg, 'ZZZZZ')).rejects.toThrow(ReleasesApiError);
  });

  it('throws ReleasesApiError when fetch itself rejects (network failure)', async () => {
    (global.fetch as any).mockRejectedValue(new Error('ECONNREFUSED'));
    await expect(deleteRelease(cfg, 'TEST0')).rejects.toThrow(ReleasesApiError);
  });
});

describe('publishToWeb 409 handling', () => {
  beforeEach(() => {
    global.fetch = vi.fn();
  });
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("carries the server's reason so a number collision is not shown as 'already published'", async () => {
    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 409,
      json: async () => ({
        error: 'WordPress already has a post numbered #217 ("#217: Hypocrisy", https://www.uprough.net/217-hypocrisy/)',
        reason: 'number-taken',
        wordpressPostUrl: 'https://www.uprough.net/217-hypocrisy/',
      }),
    });
    try {
      await publishToWeb(cfg, 'TEST0', __filename, __filename, []);
      throw new Error('expected publishToWeb to reject');
    } catch (err) {
      expect((err as ReleasesApiError).reason).toBe('number-taken');
      expect((err as ReleasesApiError).status).toBe(409);
    }
  });
});

describe('publishRelease', () => {
  describe('the DIZ it sends to the catalog', () => {
    beforeEach(() => {
      global.fetch = vi.fn();
    });
    afterEach(() => {
      vi.restoreAllMocks();
    });

    it('keeps leading spaces and drops the grid padding, exactly like the file_id.diz', async () => {
      (global.fetch as any).mockResolvedValue({
        ok: true,
        status: 200,
        json: async () => ({ releaseNumber: 1, revision: 1, history: [] }),
      });
      await publishRelease(cfg, { ...sampleForm, dizBannerText: '   /\\   \n  /__\\  \n     \n' });
      const [, init] = (global.fetch as any).mock.calls[0];
      expect(JSON.parse(init.body).dizText).toBe('   /\\\n  /__\\');
    });
  });

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

    const result = await publishToWeb(cfg, 'AAAAA', archivePath, imagePath, []);

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
    await expect(publishToWeb(cfg, 'AAAAA', '/tmp/mr-test-archive2.lha', '/tmp/mr-test-image2.png', [])).rejects.toThrow(
      'already published',
    );
    fs.unlinkSync('/tmp/mr-test-archive2.lha');
    fs.unlinkSync('/tmp/mr-test-image2.png');
  });

  it('uses fs.openAsBlob to attach the archive/image as file-backed Blobs, without reading their bytes into memory', async () => {
    const archivePath = '/tmp/mr-test-archive3.lha';
    const imagePath = '/tmp/mr-test-image3.png';
    fs.writeFileSync(archivePath, 'fake archive bytes');
    fs.writeFileSync(imagePath, 'fake image bytes');

    const body = { archiveUrl: 'https://www.uprough.net/releases/UP-AAAAA.LHA', wordpressPostUrl: 'https://www.uprough.net/x/' };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    await publishToWeb(cfg, 'AAAAA', archivePath, imagePath, []);

    const [, init] = (global.fetch as any).mock.calls[0];
    const sentForm = init.body as FormData;
    const archiveEntry = sentForm.get('archive') as Blob;
    const imageEntry = sentForm.get('image') as Blob;
    // fs.openAsBlob-backed Blobs still report the correct size/type without
    // ever having been read into a Buffer by publishToWeb itself.
    expect(archiveEntry.size).toBe(Buffer.byteLength('fake archive bytes'));
    expect(imageEntry.size).toBe(Buffer.byteLength('fake image bytes'));
    expect(await archiveEntry.text()).toBe('fake archive bytes');
    expect(await imageEntry.text()).toBe('fake image bytes');

    fs.unlinkSync(archivePath);
    fs.unlinkSync(imagePath);
  });

  it('attaches embedImage fields in order when embedImagePaths is non-empty', async () => {
    const archivePath = '/tmp/mr-test-archive5.lha';
    const imagePath = '/tmp/mr-test-image5.png';
    const embed1 = '/tmp/mr-test-embed1.png';
    const embed2 = '/tmp/mr-test-embed2.png';
    fs.writeFileSync(archivePath, 'a');
    fs.writeFileSync(imagePath, 'i');
    fs.writeFileSync(embed1, 'first embed');
    fs.writeFileSync(embed2, 'second embed');

    const body = { archiveUrl: 'https://x/UP-AAAAA.LHA', wordpressPostUrl: 'https://x/p/' };
    (global.fetch as any).mockResolvedValue({ ok: true, status: 200, json: async () => body });

    await publishToWeb(cfg, 'AAAAA', archivePath, imagePath, [embed1, embed2]);

    const [, init] = (global.fetch as any).mock.calls[0];
    const sentForm = init.body as FormData;
    const embedEntries = sentForm.getAll('embedImage') as Blob[];
    expect(embedEntries).toHaveLength(2);
    expect(await embedEntries[0].text()).toBe('first embed');
    expect(await embedEntries[1].text()).toBe('second embed');

    fs.unlinkSync(archivePath);
    fs.unlinkSync(imagePath);
    fs.unlinkSync(embed1);
    fs.unlinkSync(embed2);
  });

  it('attaches status and wordpressPostUrl to the thrown ReleasesApiError on a 409 "already published" response', async () => {
    const archivePath = '/tmp/mr-test-archive4.lha';
    const imagePath = '/tmp/mr-test-image4.png';
    fs.writeFileSync(archivePath, 'a');
    fs.writeFileSync(imagePath, 'i');

    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 409,
      json: async () => ({ error: 'already published', wordpressPostUrl: 'https://www.uprough.net/x/' }),
    });

    let caught: ReleasesApiError | undefined;
    try {
      await publishToWeb(cfg, 'AAAAA', archivePath, imagePath, []);
    } catch (err) {
      caught = err as ReleasesApiError;
    }

    expect(caught).toBeInstanceOf(ReleasesApiError);
    expect(caught?.status).toBe(409);
    expect(caught?.wordpressPostUrl).toBe('https://www.uprough.net/x/');
    expect(caught?.message).toBe('already published');

    fs.unlinkSync(archivePath);
    fs.unlinkSync(imagePath);
  });

  it('does not attach wordpressPostUrl for a non-409 failure, even if status is set', async () => {
    const archivePath = '/tmp/mr-test-archive5.lha';
    const imagePath = '/tmp/mr-test-image5.png';
    fs.writeFileSync(archivePath, 'a');
    fs.writeFileSync(imagePath, 'i');

    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 500,
      json: async () => ({ error: 'internal error', wordpressPostUrl: 'https://should-not-appear.example/' }),
    });

    let caught: ReleasesApiError | undefined;
    try {
      await publishToWeb(cfg, 'AAAAA', archivePath, imagePath, []);
    } catch (err) {
      caught = err as ReleasesApiError;
    }

    expect(caught?.status).toBe(500);
    expect(caught?.wordpressPostUrl).toBeUndefined();

    fs.unlinkSync(archivePath);
    fs.unlinkSync(imagePath);
  });
});

describe('clientConfigFor', () => {
  const profile = { id: 'uprough', server: { baseUrl: 'https://releases.example', writeKey: 'from-profile' } };

  it('takes the server and key from the profile', () => {
    expect(clientConfigFor(profile, {})).toEqual({ baseUrl: 'https://releases.example', writeKey: 'from-profile' });
  });

  it('lets the environment override both, for local development against another server', () => {
    expect(
      clientConfigFor(profile, { RELEASES_API_BASE_URL: 'http://localhost:8787', RELEASES_WRITE_KEY: 'dev' }),
    ).toEqual({ baseUrl: 'http://localhost:8787', writeKey: 'dev' });
  });

  it('passes an empty key through, so loadSeedState can warn rather than fail', () => {
    expect(clientConfigFor({ id: 'x', server: { baseUrl: 'https://r', writeKey: '' } }, {}).writeKey).toBe('');
  });

  it('refuses a profile without a server block', () => {
    expect(() => clientConfigFor({ id: 'example' }, {})).toThrow(/"example" has no server block/);
  });
});
