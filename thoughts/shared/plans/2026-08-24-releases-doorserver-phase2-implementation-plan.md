# releases-doorserver Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Publish" pipeline to `releases-doorserver` (copy the built archive into `www.uprough.net`'s live `/releases/` directory, create a WordPress announcement post with a cover image) and wire a matching explicit "Publish" step into the `makerelease` app, separate from "Build".

**Architecture:** A new authenticated, multipart `POST /api/releases/:code/publish` endpoint on the existing `releases-doorserver` (both projects already exist and are fully working - this plan only adds to them). The endpoint streams the incoming archive straight to `WORDPRESS_RELEASES_DIR` (a local path - `releases-doorserver` and `www.uprough.net` share a host, confirmed, so this is a filesystem copy, not SSH/SCP), uploads the image and creates the post via the WordPress REST API using an Application Password, and records the result on the release row for idempotency. The app gets a new image-picker field and a "Publish" button, enabled only after a successful "Build", that multipart-POSTs to this endpoint.

**Tech Stack:** Same as Phase 1 (TypeScript, Express, better-sqlite3, Jest+supertest on the server; Electron/TypeScript/Vitest on the app), plus `busboy` (multipart parsing, same library the sibling `amiexpress-doorserver` project already uses for its own upload endpoint - reuse its pattern, don't reinvent).

**Spec:** `thoughts/shared/plans/2026-08-24-releases-doorserver-phase2-publish-design.md`

## Global Constraints

- `WORDPRESS_RELEASES_DIR`, `WORDPRESS_URL`, `WORDPRESS_USERNAME`, `WORDPRESS_APP_PASSWORD` are all required config (fail-fast, no defaults - same policy as every Phase 1 credential).
- The archive is **streamed directly to disk**, never fully buffered in memory - real archives run 100+ MB (one real release in this project is ~181 MB), and buffering that per-request risks the server's memory under concurrent uploads.
- No test may perform a real WordPress API call or write outside a temp directory - all WordPress HTTP calls are mocked in the automated test suite (matching Phase 1's and the sibling project's existing conventions); a real WordPress call only happens in the documented manual end-to-end step.
- `POST /:code/publish` requires `:code` to already exist as a `releases` row from Phase 1's `POST /releases` - it reads the release's metadata from that row rather than accepting it again in the multipart body, so publish and the original metadata can never disagree.
- Category → WordPress category mapping is the exact 1:1 table in the spec (Demo→Demos, Intro→Intros, Musicdisk→Musicdisks, Music→Music, Graphics→Graphics, ASCII→Ascii, Tool→Tools, Video→Videos, Science→Science, Other→Other) - never guessed or pluralized generically.
- Every bug fix / new behavior ships with a regression test that fails before and passes after.

---

## Part A — `releases-doorserver` additions

### Task 1: Schema + config additions

**Files:**
- Modify: `/Users/spot/Code/releases-doorserver/src/schema.sql`
- Modify: `/Users/spot/Code/releases-doorserver/src/config.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/config.test.ts`

**Interfaces:**
- Produces: `releases` table gains `published_at INTEGER` and `wordpress_post_url TEXT` (both nullable - `NULL` means not yet published). `ServerConfig` gains `wordpress: { releasesDir: string; url: string; username: string; appPassword: string }`.

- [ ] **Step 1: Update the schema**

Add to the `CREATE TABLE releases (...)` statement in `src/schema.sql`, after `created_at`:

```sql
  published_at       INTEGER,
  wordpress_post_url TEXT
```

(No migration machinery needed - `applySchema` is `CREATE TABLE IF NOT EXISTS` on boot, and there's no live production data yet per Phase 1's own convention.)

- [ ] **Step 2: Write the failing config tests**

Add to `tests/config.test.ts` (alongside the existing `describe('loadConfig', ...)` block, same file, same helper functions):

```typescript
it('throws when WORDPRESS_RELEASES_DIR is not set', () => {
  expect(() =>
    loadConfig({
      RELEASES_DB: tmpDb,
      RELEASES_WRITE_KEYS: 'app:secret',
      WORDPRESS_URL: 'https://www.uprough.net',
      WORDPRESS_USERNAME: 'svc',
      WORDPRESS_APP_PASSWORD: 'xxxx xxxx xxxx xxxx xxxx xxxx',
    }),
  ).toThrow(ConfigError);
});

it('throws when WORDPRESS_RELEASES_DIR points at a nonexistent path', () => {
  expect(() =>
    loadConfig({
      RELEASES_DB: tmpDb,
      RELEASES_WRITE_KEYS: 'app:secret',
      WORDPRESS_RELEASES_DIR: '/nonexistent/releases-dir',
      WORDPRESS_URL: 'https://www.uprough.net',
      WORDPRESS_USERNAME: 'svc',
      WORDPRESS_APP_PASSWORD: 'xxxx xxxx xxxx xxxx xxxx xxxx',
    }),
  ).toThrow(ConfigError);
});

it('throws when WORDPRESS_URL/USERNAME/APP_PASSWORD are missing', () => {
  const releasesDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rds-wpdir-'));
  expect(() =>
    loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:secret', WORDPRESS_RELEASES_DIR: releasesDir }),
  ).toThrow(ConfigError);
});

it('parses valid wordpress config into cfg.wordpress', () => {
  const releasesDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rds-wpdir-'));
  const cfg = loadConfig({
    RELEASES_DB: tmpDb,
    RELEASES_WRITE_KEYS: 'app:secret',
    WORDPRESS_RELEASES_DIR: releasesDir,
    WORDPRESS_URL: 'https://www.uprough.net',
    WORDPRESS_USERNAME: 'svc',
    WORDPRESS_APP_PASSWORD: 'xxxx xxxx xxxx xxxx xxxx xxxx',
  });
  expect(cfg.wordpress).toEqual({
    releasesDir,
    url: 'https://www.uprough.net',
    username: 'svc',
    appPassword: 'xxxx xxxx xxxx xxxx xxxx xxxx',
  });
});
```

- [ ] **Step 2b: Run tests to verify they fail**

Run: `npm test -- config.test.ts`
Expected: FAIL - `cfg.wordpress` is `undefined`, or a thrown error where none is expected yet.

- [ ] **Step 3: Extend `config.ts`**

```typescript
export interface WordPressConfig {
  releasesDir: string;
  url: string;
  username: string;
  appPassword: string;
}

// add to ServerConfig:
//   wordpress: WordPressConfig;
```

In `loadConfig`, after the existing `port` computation:

```typescript
const releasesDir = mustExist('WORDPRESS_RELEASES_DIR', required(env, 'WORDPRESS_RELEASES_DIR'));
const wordpress: WordPressConfig = {
  releasesDir,
  url: required(env, 'WORDPRESS_URL'),
  username: required(env, 'WORDPRESS_USERNAME'),
  appPassword: required(env, 'WORDPRESS_APP_PASSWORD'),
};
```

Add `wordpress` to the returned object and to the `ServerConfig` interface.

- [ ] **Step 4: Run tests, typecheck**

Run: `npm test -- config.test.ts && npm run typecheck`
Expected: PASS. (Existing `tests/config.test.ts` cases that don't pass `WORDPRESS_*` vars will now fail unless you update them - add the four `WORDPRESS_*` vars to every existing passing-case test in this file, e.g. the "parses a single label:key pair" test, so they keep passing. Run the full file and fix every case, not just the new ones.)

- [ ] **Step 5: Commit**

```bash
git add src/schema.sql src/config.ts tests/config.test.ts
git commit -m "Add published_at/wordpress_post_url columns and WordPress config"
```

---

### Task 2: Catalog additions (read-by-code, mark-published, category mapping)

**Files:**
- Modify: `/Users/spot/Code/releases-doorserver/src/catalog.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/catalog.test.ts`

**Interfaces:**
- Consumes: `Database.Database`.
- Produces:
  - `interface ReleaseRow { code: string; releaseNumber: number; releaseNumberDisplay: string; format: string; releaseName: string; releaseNotes: string; entryCategory: string; publishedAt: number | null; wordpressPostUrl: string | null }`
  - `getReleaseByCode(db, code: string): ReleaseRow | null`
  - `markPublished(db, code: string, wordpressPostUrl: string): void`
  - `export const CATEGORY_TO_WORDPRESS: Record<string, string>` - the exact 10-entry mapping table from the spec.

- [ ] **Step 1: Write the failing tests**

```typescript
// add to tests/catalog.test.ts
import { getReleaseByCode, markPublished, CATEGORY_TO_WORDPRESS } from '../src/catalog';

describe('getReleaseByCode', () => {
  it('returns null for an unknown code', () => {
    const db = freshDb();
    expect(getReleaseByCode(db, 'ZZZZZ')).toBeNull();
    db.close();
  });

  it('returns the release row with publishedAt/wordpressPostUrl null before publishing', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    const row = getReleaseByCode(db, 'AAAAA');
    expect(row?.code).toBe('AAAAA');
    expect(row?.publishedAt).toBeNull();
    expect(row?.wordpressPostUrl).toBeNull();
  });
});

describe('markPublished', () => {
  it('sets publishedAt and wordpressPostUrl', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    markPublished(db, 'AAAAA', 'https://www.uprough.net/some-post/');
    const row = getReleaseByCode(db, 'AAAAA');
    expect(row?.publishedAt).toEqual(expect.any(Number));
    expect(row?.wordpressPostUrl).toBe('https://www.uprough.net/some-post/');
  });
});

describe('CATEGORY_TO_WORDPRESS', () => {
  it('maps every app category to its real WordPress category name', () => {
    expect(CATEGORY_TO_WORDPRESS).toEqual({
      Demo: 'Demos',
      Intro: 'Intros',
      Musicdisk: 'Musicdisks',
      Music: 'Music',
      Graphics: 'Graphics',
      ASCII: 'Ascii',
      Tool: 'Tools',
      Video: 'Videos',
      Science: 'Science',
      Other: 'Other',
    });
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- catalog.test.ts`
Expected: FAIL - `getReleaseByCode`/`markPublished`/`CATEGORY_TO_WORDPRESS` not exported yet.

- [ ] **Step 3: Implement in `catalog.ts`**

```typescript
export const CATEGORY_TO_WORDPRESS: Record<string, string> = {
  Demo: 'Demos',
  Intro: 'Intros',
  Musicdisk: 'Musicdisks',
  Music: 'Music',
  Graphics: 'Graphics',
  ASCII: 'Ascii',
  Tool: 'Tools',
  Video: 'Videos',
  Science: 'Science',
  Other: 'Other',
};

export interface ReleaseRow {
  code: string;
  releaseNumber: number;
  releaseNumberDisplay: string;
  format: string;
  releaseName: string;
  releaseNotes: string;
  entryCategory: string;
  publishedAt: number | null;
  wordpressPostUrl: string | null;
}

export function getReleaseByCode(db: Database.Database, code: string): ReleaseRow | null {
  const row = db
    .prepare(
      `SELECT code, release_number as releaseNumber, release_number_display as releaseNumberDisplay,
              format, release_name as releaseName, release_notes as releaseNotes,
              entry_category as entryCategory, published_at as publishedAt, wordpress_post_url as wordpressPostUrl
       FROM releases WHERE code = ?`,
    )
    .get(code) as ReleaseRow | undefined;
  return row ?? null;
}

export function markPublished(db: Database.Database, code: string, wordpressPostUrl: string): void {
  db.prepare('UPDATE releases SET published_at = strftime(\'%s\',\'now\'), wordpress_post_url = ? WHERE code = ?').run(
    wordpressPostUrl,
    code,
  );
}
```

- [ ] **Step 4: Run tests, typecheck**

Run: `npm test -- catalog.test.ts && npm run typecheck`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/catalog.ts tests/catalog.test.ts
git commit -m "Add getReleaseByCode, markPublished, and the category->WordPress mapping"
```

---

### Task 3: WordPress REST client

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/wordpress-client.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/wordpress-client.test.ts`

**Interfaces:**
- Consumes: `WordPressConfig` (Task 1).
- Produces:
  - `class WordPressError extends Error`
  - `uploadMedia(cfg: WordPressConfig, imageBytes: Buffer, filename: string): Promise<number>` (returns the WP media ID)
  - `findCategoryId(cfg: WordPressConfig, categoryName: string): Promise<number>` (throws `WordPressError` if not found)
  - `createPost(cfg: WordPressConfig, input: { title: string; body: string; categoryId: number; featuredMediaId: number }): Promise<{ id: number; url: string }>`

This module uses Node's built-in global `fetch` (same as `releases-client.ts` in the sibling `makerelease/app` project - no new HTTP dependency). All three functions authenticate with HTTP Basic Auth using `cfg.wordpress.username`/`cfg.wordpress.appPassword` (WordPress Application Passwords use HTTP Basic Auth, per WP's own REST API convention - `Authorization: Basic <base64(username:appPassword)>`).

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/wordpress-client.test.ts
import { describe, it, expect, jest, beforeEach, afterEach } from '@jest/globals';
import { uploadMedia, findCategoryId, createPost, WordPressError } from '../src/wordpress-client';
import type { WordPressConfig } from '../src/config';

const cfg: WordPressConfig = {
  releasesDir: '/tmp',
  url: 'https://www.uprough.test',
  username: 'svc',
  appPassword: 'xxxx xxxx xxxx xxxx xxxx xxxx',
};

describe('uploadMedia', () => {
  beforeEach(() => {
    global.fetch = jest.fn() as any;
  });
  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('POSTs to /wp-json/wp/v2/media with Basic auth and returns the media id', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      status: 201,
      json: async () => ({ id: 42 }),
    });
    const id = await uploadMedia(cfg, Buffer.from('fake image bytes'), 'cover.png');
    expect(id).toBe(42);
    const [url, init] = (global.fetch as jest.Mock).mock.calls[0];
    expect(url).toBe('https://www.uprough.test/wp-json/wp/v2/media');
    expect(init.method).toBe('POST');
    expect(init.headers['Authorization']).toBe(
      `Basic ${Buffer.from('svc:xxxx xxxx xxxx xxxx xxxx xxxx').toString('base64')}`,
    );
    expect(init.headers['Content-Disposition']).toContain('cover.png');
  });

  it('throws WordPressError on a non-2xx response', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: false,
      status: 401,
      json: async () => ({ message: 'invalid credentials' }),
    });
    await expect(uploadMedia(cfg, Buffer.from('x'), 'x.png')).rejects.toThrow(WordPressError);
  });
});

describe('findCategoryId', () => {
  beforeEach(() => {
    global.fetch = jest.fn() as any;
  });
  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('GETs /wp-json/wp/v2/categories?search=<name> and returns the matching id', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      status: 200,
      json: async () => [{ id: 7, name: 'Demos' }],
    });
    const id = await findCategoryId(cfg, 'Demos');
    expect(id).toBe(7);
    const [url] = (global.fetch as jest.Mock).mock.calls[0];
    expect(url).toContain('/wp-json/wp/v2/categories?search=Demos');
  });

  it('throws WordPressError when no category matches', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({ ok: true, status: 200, json: async () => [] });
    await expect(findCategoryId(cfg, 'Nonexistent')).rejects.toThrow(WordPressError);
  });

  it('matches case-insensitively but exactly (not a substring match on a different category)', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      status: 200,
      json: async () => [
        { id: 3, name: 'Musicdisks' },
        { id: 4, name: 'Music' },
      ],
    });
    const id = await findCategoryId(cfg, 'Music');
    expect(id).toBe(4);
  });
});

describe('createPost', () => {
  beforeEach(() => {
    global.fetch = jest.fn() as any;
  });
  afterEach(() => {
    jest.restoreAllMocks();
  });

  it('POSTs to /wp-json/wp/v2/posts with title/content/categories/featured_media and returns id+url', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({
      ok: true,
      status: 201,
      json: async () => ({ id: 99, link: 'https://www.uprough.test/hypocrisy/' }),
    });
    const result = await createPost(cfg, { title: '#217: Hypocrisy', body: 'notes', categoryId: 7, featuredMediaId: 42 });
    expect(result).toEqual({ id: 99, url: 'https://www.uprough.test/hypocrisy/' });
    const [url, init] = (global.fetch as jest.Mock).mock.calls[0];
    expect(url).toBe('https://www.uprough.test/wp-json/wp/v2/posts');
    const body = JSON.parse(init.body);
    expect(body).toEqual({
      title: '#217: Hypocrisy',
      content: 'notes',
      categories: [7],
      featured_media: 42,
      status: 'publish',
    });
  });

  it('throws WordPressError on a non-2xx response', async () => {
    (global.fetch as jest.Mock).mockResolvedValue({ ok: false, status: 400, json: async () => ({ message: 'bad' }) });
    await expect(createPost(cfg, { title: 't', body: 'b', categoryId: 1, featuredMediaId: 1 })).rejects.toThrow(
      WordPressError,
    );
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- wordpress-client.test.ts`
Expected: FAIL - module not found.

- [ ] **Step 3: Implement `src/wordpress-client.ts`**

```typescript
import type { WordPressConfig } from './config';

export class WordPressError extends Error {}

function authHeader(cfg: WordPressConfig): string {
  return `Basic ${Buffer.from(`${cfg.username}:${cfg.appPassword}`).toString('base64')}`;
}

async function wpFetch(cfg: WordPressConfig, path: string, init: RequestInit): Promise<any> {
  let response: Response;
  try {
    response = await fetch(`${cfg.url}${path}`, {
      ...init,
      headers: { ...(init.headers as Record<string, string>), Authorization: authHeader(cfg) },
    });
  } catch (err) {
    throw new WordPressError(`could not reach WordPress at ${cfg.url}: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new WordPressError(`WordPress ${path} returned ${response.status}: ${body.message ?? 'unknown error'}`);
  }
  return body;
}

export async function uploadMedia(cfg: WordPressConfig, imageBytes: Buffer, filename: string): Promise<number> {
  const body = await wpFetch(cfg, '/wp-json/wp/v2/media', {
    method: 'POST',
    headers: {
      'Content-Disposition': `attachment; filename="${filename}"`,
      'Content-Type': 'application/octet-stream',
    },
    body: imageBytes,
  });
  return body.id;
}

export async function findCategoryId(cfg: WordPressConfig, categoryName: string): Promise<number> {
  const body = await wpFetch(cfg, `/wp-json/wp/v2/categories?search=${encodeURIComponent(categoryName)}`, {
    method: 'GET',
  });
  const match = (body as Array<{ id: number; name: string }>).find(
    (c) => c.name.toLowerCase() === categoryName.toLowerCase(),
  );
  if (!match) {
    throw new WordPressError(`no WordPress category found matching "${categoryName}"`);
  }
  return match.id;
}

export async function createPost(
  cfg: WordPressConfig,
  input: { title: string; body: string; categoryId: number; featuredMediaId: number },
): Promise<{ id: number; url: string }> {
  const body = await wpFetch(cfg, '/wp-json/wp/v2/posts', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      title: input.title,
      content: input.body,
      categories: [input.categoryId],
      featured_media: input.featuredMediaId,
      status: 'publish',
    }),
  });
  return { id: body.id, url: body.link };
}
```

- [ ] **Step 4: Run tests, typecheck**

Run: `npm test -- wordpress-client.test.ts && npm run typecheck`
Expected: PASS, all 8 tests.

- [ ] **Step 5: Commit**

```bash
git add src/wordpress-client.ts tests/wordpress-client.test.ts
git commit -m "Add WordPress REST client (media upload, category lookup, post creation)"
```

---

### Task 4: Multipart upload handling (stream archive to disk, buffer image)

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/upload.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/upload.test.ts`
- Modify: `/Users/spot/Code/releases-doorserver/package.json` (add `busboy` + `@types/busboy` dependencies)

**Interfaces:**
- Produces:
  - `class UploadError extends Error { status: number }`
  - `interface ReceivedPublishUpload { archivePath: string; imageBytes: Buffer; imageFilename: string }`
  - `receivePublishUpload(req: Request, tmpDir: string): Promise<ReceivedPublishUpload>` - parses a multipart request with two file fields (`archive`, `image`); streams `archive` directly to a file inside `tmpDir` (never buffered in memory - real archives run 100+ MB) and returns its path; buffers `image` in memory (small, a cover photo) and returns its bytes + original filename.

This closely follows the sibling `amiexpress-doorserver` project's `receiveUpload` pattern in `src/submissions.ts` (read it for the busboy event-wiring shape - `on('field')`/`on('file')`/`on('error')`/`on('close')`, the `fail()` guard-against-double-reject helper) but adapted for TWO file fields instead of one, and the `archive` field streamed to disk instead of buffered.

- [ ] **Step 1: Add the dependency**

```bash
npm install busboy@^1.6.0
npm install --save-dev @types/busboy@^1.5.4
```

(Match the exact versions the sibling project pins, per `grep busboy /Users/spot/Code/amiexpress-doorserver/package.json`.)

- [ ] **Step 2: Write the failing tests**

```typescript
// tests/upload.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import express from 'express';
import request from 'supertest';
import { receivePublishUpload, UploadError } from '../src/upload';

function testApp(tmpDir: string) {
  const app = express();
  app.post('/upload', async (req, res) => {
    try {
      const result = await receivePublishUpload(req, tmpDir);
      const archiveBytes = fs.readFileSync(result.archivePath);
      res.json({
        archivePath: result.archivePath,
        archiveSize: archiveBytes.length,
        imageSize: result.imageBytes.length,
        imageFilename: result.imageFilename,
      });
    } catch (err) {
      const status = err instanceof UploadError ? err.status : 500;
      res.status(status).json({ error: (err as Error).message });
    }
  });
  return app;
}

describe('receivePublishUpload', () => {
  let tmpDir: string;

  beforeEach(() => {
    tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rds-upload-'));
  });
  afterEach(() => {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  it('streams the archive field to a file in tmpDir and buffers the image field', async () => {
    const res = await request(testApp(tmpDir))
      .post('/upload')
      .attach('archive', Buffer.from('fake archive bytes, could be huge'), 'UP-TEST0.LHA')
      .attach('image', Buffer.from('fake image bytes'), 'cover.png');
    expect(res.status).toBe(200);
    expect(res.body.archiveSize).toBe(Buffer.byteLength('fake archive bytes, could be huge'));
    expect(res.body.imageSize).toBe(Buffer.byteLength('fake image bytes'));
    expect(res.body.imageFilename).toBe('cover.png');
    expect(res.body.archivePath.startsWith(tmpDir)).toBe(true);
    expect(fs.existsSync(res.body.archivePath)).toBe(true);
  });

  it('rejects with UploadError when the archive field is missing', async () => {
    const res = await request(testApp(tmpDir)).post('/upload').attach('image', Buffer.from('x'), 'cover.png');
    expect(res.status).toBe(400);
  });

  it('rejects with UploadError when the image field is missing', async () => {
    const res = await request(testApp(tmpDir)).post('/upload').attach('archive', Buffer.from('x'), 'UP-TEST0.LHA');
    expect(res.status).toBe(400);
  });

  it('rejects a non-multipart request', async () => {
    const res = await request(testApp(tmpDir)).post('/upload').send({ not: 'multipart' });
    expect(res.status).toBe(400);
  });
});
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `npm test -- upload.test.ts`
Expected: FAIL - module not found.

- [ ] **Step 4: Implement `src/upload.ts`**

```typescript
import Busboy from 'busboy';
import * as crypto from 'crypto';
import * as fs from 'fs';
import * as path from 'path';
import type { Request } from 'express';

export class UploadError extends Error {
  status: number;
  constructor(message: string, status: number) {
    super(message);
    this.status = status;
  }
}

export interface ReceivedPublishUpload {
  archivePath: string;
  imageBytes: Buffer;
  imageFilename: string;
}

export function receivePublishUpload(req: Request, tmpDir: string): Promise<ReceivedPublishUpload> {
  return new Promise((resolve, reject) => {
    let busboy: ReturnType<typeof Busboy>;
    try {
      busboy = Busboy({ headers: req.headers });
    } catch {
      reject(new UploadError('expected a multipart upload', 400));
      return;
    }

    let archivePath: string | null = null;
    let archiveWriteStream: fs.WriteStream | null = null;
    let archiveSeen = false;
    const imageChunks: Buffer[] = [];
    let imageFilename: string | null = null;
    let failed = false;

    const fail = (error: UploadError) => {
      if (failed) return;
      failed = true;
      req.unpipe(busboy);
      if (archiveWriteStream) archiveWriteStream.destroy();
      if (archivePath) fs.rm(archivePath, { force: true }, () => {});
      reject(error);
    };

    busboy.on('file', (name, stream, info) => {
      if (name === 'archive') {
        archiveSeen = true;
        archivePath = path.join(tmpDir, `archive-${crypto.randomUUID()}`);
        archiveWriteStream = fs.createWriteStream(archivePath);
        stream.pipe(archiveWriteStream);
        stream.on('error', () => fail(new UploadError('failed reading the archive upload', 400)));
      } else if (name === 'image') {
        imageFilename = info.filename;
        stream.on('data', (chunk: Buffer) => imageChunks.push(chunk));
        stream.on('error', () => fail(new UploadError('failed reading the image upload', 400)));
      } else {
        stream.resume(); // drain and ignore unexpected fields
      }
    });

    busboy.on('error', () => fail(new UploadError('that upload could not be read', 400)));

    busboy.on('close', () => {
      if (failed) return;
      if (!archiveSeen || !archivePath) {
        reject(new UploadError('no archive file was attached', 400));
        return;
      }
      if (!imageFilename) {
        reject(new UploadError('no image file was attached', 400));
        return;
      }
      resolve({ archivePath, imageBytes: Buffer.concat(imageChunks), imageFilename });
    });

    req.pipe(busboy);
  });
}
```

- [ ] **Step 5: Run tests, typecheck**

Run: `npm test -- upload.test.ts && npm run typecheck`
Expected: PASS, all 4 tests.

- [ ] **Step 6: Commit**

```bash
git add package.json package-lock.json src/upload.ts tests/upload.test.ts
git commit -m "Add multipart upload handling, streaming the archive to disk"
```

---

### Task 5: `POST /:code/publish` route

**Files:**
- Modify: `/Users/spot/Code/releases-doorserver/src/routes.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/routes.test.ts`

**Interfaces:**
- Consumes: `getReleaseByCode`/`markPublished`/`CATEGORY_TO_WORDPRESS` (Task 2), `uploadMedia`/`findCategoryId`/`createPost`/`WordPressError` (Task 3), `receivePublishUpload`/`UploadError` (Task 4), `requireWriteKey` (Phase 1).
- Produces: `POST /:code/publish` mounted on the existing router.

- [ ] **Step 1: Write the failing tests**

Add to `tests/routes.test.ts` (reuse this file's existing `testAppAndConfig()` helper - it already uses the real `createApp(cfg)` per Phase 1's final-review fix, and already inserts a release via the existing `validBody()` helper before these new tests run):

```typescript
import * as fs from 'fs';
import * as wordpressClient from '../src/wordpress-client';

jest.mock('../src/wordpress-client');

describe('POST /api/releases/:code/publish', () => {
  beforeEach(() => {
    jest.resetAllMocks();
  });

  it('rejects with 401 when no X-Release-Key is provided', async () => {
    const app = testAppAndConfig();
    const res = await request(app)
      .post('/api/releases/AAAAA/publish')
      .attach('archive', Buffer.from('archive bytes'), 'UP-AAAAA.LHA')
      .attach('image', Buffer.from('image bytes'), 'cover.png');
    expect(res.status).toBe(401);
  });

  it('returns 404 when :code does not exist as a release', async () => {
    const app = testAppAndConfig();
    const res = await request(app)
      .post('/api/releases/ZZZZZ/publish')
      .set('X-Release-Key', 'secret')
      .attach('archive', Buffer.from('a'), 'UP-ZZZZZ.LHA')
      .attach('image', Buffer.from('i'), 'cover.png');
    expect(res.status).toBe(404);
  });

  it('on success: copies the archive to WORDPRESS_RELEASES_DIR, creates the post, marks published, and returns archiveUrl+wordpressPostUrl', async () => {
    const app = testAppAndConfig(); // inserts release 'AAAAA' via validBody() as part of its own setup — see existing beforeEach pattern in this file; ensure that release exists before this test runs, matching how other POST tests in this file already rely on a prior insert
    (wordpressClient.findCategoryId as jest.Mock).mockResolvedValue(7);
    (wordpressClient.uploadMedia as jest.Mock).mockResolvedValue(42);
    (wordpressClient.createPost as jest.Mock).mockResolvedValue({ id: 99, url: 'https://www.uprough.test/aaaaa/' });

    const res = await request(app)
      .post('/api/releases/AAAAA/publish')
      .set('X-Release-Key', 'secret')
      .attach('archive', Buffer.from('archive bytes'), 'UP-AAAAA.LHA')
      .attach('image', Buffer.from('image bytes'), 'cover.png');

    expect(res.status).toBe(200);
    expect(res.body.wordpressPostUrl).toBe('https://www.uprough.test/aaaaa/');
    expect(res.body.archiveUrl).toContain('UP-AAAAA.LHA');
    expect(wordpressClient.findCategoryId).toHaveBeenCalledWith(expect.anything(), 'Demos'); // assuming validBody()'s entryCategory is 'Demo'
    expect(wordpressClient.createPost).toHaveBeenCalledWith(
      expect.anything(),
      expect.objectContaining({ categoryId: 7, featuredMediaId: 42, title: expect.stringMatching(/^#\d+: /) }),
    );
  });

  it('returns 409 with the existing wordpressPostUrl on a second publish attempt', async () => {
    const app = testAppAndConfig();
    (wordpressClient.findCategoryId as jest.Mock).mockResolvedValue(7);
    (wordpressClient.uploadMedia as jest.Mock).mockResolvedValue(42);
    (wordpressClient.createPost as jest.Mock).mockResolvedValue({ id: 99, url: 'https://www.uprough.test/aaaaa/' });

    await request(app)
      .post('/api/releases/AAAAA/publish')
      .set('X-Release-Key', 'secret')
      .attach('archive', Buffer.from('a'), 'UP-AAAAA.LHA')
      .attach('image', Buffer.from('i'), 'cover.png');

    const second = await request(app)
      .post('/api/releases/AAAAA/publish')
      .set('X-Release-Key', 'secret')
      .attach('archive', Buffer.from('a'), 'UP-AAAAA.LHA')
      .attach('image', Buffer.from('i'), 'cover.png');

    expect(second.status).toBe(409);
    expect(second.body.wordpressPostUrl).toBe('https://www.uprough.test/aaaaa/');
    expect(wordpressClient.createPost).toHaveBeenCalledTimes(1); // not called again on the second attempt
  });

  it('surfaces a distinct error when createPost fails after the archive copy already succeeded', async () => {
    const app = testAppAndConfig();
    (wordpressClient.findCategoryId as jest.Mock).mockResolvedValue(7);
    (wordpressClient.uploadMedia as jest.Mock).mockResolvedValue(42);
    (wordpressClient.createPost as jest.Mock).mockRejectedValue(new wordpressClient.WordPressError('WP is down'));

    const res = await request(app)
      .post('/api/releases/AAAAA/publish')
      .set('X-Release-Key', 'secret')
      .attach('archive', Buffer.from('a'), 'UP-AAAAA.LHA')
      .attach('image', Buffer.from('i'), 'cover.png');

    expect(res.status).toBe(502);
    expect(res.body.error).toContain('WP is down');
    // the archive copy is NOT undone on a later-step failure — a retry after fixing WordPress
    // should not need to re-upload the archive. Confirm the file landed in the configured dir.
    const cfg = /* the same cfg testAppAndConfig() used */;
    expect(fs.existsSync(`${cfg.wordpress.releasesDir}/UP-AAAAA.LHA`)).toBe(true);
  });
});
```

Adjust the exact mock-setup boilerplate (`testAppAndConfig()`'s signature, how `cfg` is accessed in the last test) to match whatever `testAppAndConfig()` actually looks like in the real file at this point — read it first, this is illustrative of the required test cases and their assertions, not a literal drop-in if the helper's shape has evolved. The required cases are: 401 no key, 404 unknown code, 200 full success with the right WordPress calls made, 409 idempotent re-publish, and 502 on a WordPress-step failure with the archive copy already persisted (not rolled back).

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- routes.test.ts`
Expected: FAIL - route doesn't exist yet.

- [ ] **Step 3: Implement the route in `src/routes.ts`**

```typescript
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { getReleaseByCode, markPublished, CATEGORY_TO_WORDPRESS } from './catalog';
import { uploadMedia, findCategoryId, createPost, WordPressError } from './wordpress-client';
import { receivePublishUpload, UploadError } from './upload';

// inside createRouter(cfg), alongside the existing routes:

router.post('/:code/publish', requireWriteKey(cfg), async (req: Request, res: Response) => {
  const code = String(req.params.code).toUpperCase();
  const db = openDb(cfg);
  let release;
  try {
    release = getReleaseByCode(db, code);
    if (!release) {
      res.status(404).json({ error: `no release found for code ${code}` });
      return;
    }
    if (release.publishedAt) {
      res.status(409).json({ error: 'already published', wordpressPostUrl: release.wordpressPostUrl });
      return;
    }
  } finally {
    db.close();
  }

  const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'releases-publish-'));
  try {
    const upload = await receivePublishUpload(req, tmpDir);

    const destFilename = `UP-${code}.${release.format}`;
    const destPath = path.join(cfg.wordpress.releasesDir, destFilename);
    await fs.copyFile(upload.archivePath, destPath);
    const archiveUrl = `${cfg.wordpress.url}/releases/${destFilename}`;

    const wordpressCategory = CATEGORY_TO_WORDPRESS[release.entryCategory];
    if (!wordpressCategory) {
      res.status(500).json({ error: `no WordPress category mapping for app category "${release.entryCategory}"` });
      return;
    }

    const categoryId = await findCategoryId(cfg.wordpress, wordpressCategory);
    const mediaId = await uploadMedia(cfg.wordpress, upload.imageBytes, upload.imageFilename);
    const post = await createPost(cfg.wordpress, {
      title: `#${release.releaseNumberDisplay}: ${release.releaseName}`,
      body: release.releaseNotes,
      categoryId,
      featuredMediaId: mediaId,
    });

    const db2 = openDb(cfg);
    try {
      markPublished(db2, code, post.url);
    } finally {
      db2.close();
    }

    res.json({ archiveUrl, wordpressPostUrl: post.url });
  } catch (err) {
    if (err instanceof UploadError) {
      res.status(err.status).json({ error: err.message });
      return;
    }
    if (err instanceof WordPressError) {
      res.status(502).json({ error: err.message });
      return;
    }
    throw err;
  } finally {
    await fs.rm(tmpDir, { recursive: true, force: true });
  }
});
```

(Adjust imports/types to match this file's actual existing style - e.g. if `Request`/`Response` are already imported at the top of the file, don't re-import; follow the existing route handlers' exact conventions for consistency.)

- [ ] **Step 4: Run tests, typecheck**

Run: `npm test -- routes.test.ts && npm run typecheck`
Expected: PASS, all new tests plus every pre-existing test in the file still green.

- [ ] **Step 5: Commit**

```bash
git add src/routes.ts tests/routes.test.ts
git commit -m "Add POST /:code/publish: archive copy + WordPress post creation"
```

---

### Task 6: Dockerfile / docker-compose / README updates

**Files:**
- Modify: `/Users/spot/Code/releases-doorserver/docker-compose.yml`
- Modify: `/Users/spot/Code/releases-doorserver/README.md`

**Interfaces:** none - config/documentation only.

- [ ] **Step 1: Add the four new env vars to `docker-compose.yml`**

```yaml
      WORDPRESS_RELEASES_DIR: ${WORDPRESS_RELEASES_DIR}
      WORDPRESS_URL: ${WORDPRESS_URL}
      WORDPRESS_USERNAME: ${WORDPRESS_USERNAME}
      WORDPRESS_APP_PASSWORD: ${WORDPRESS_APP_PASSWORD}
```

Also add a bind mount so the container can actually write into the real `www.uprough.net` releases directory on the shared host (this is a same-host local path, not a network mount - Phase 2's whole point):

```yaml
    volumes:
      - releases-data:/data
      - ${WORDPRESS_RELEASES_DIR}:${WORDPRESS_RELEASES_DIR}
```

- [ ] **Step 2: Update `README.md`'s Configuration table**

Add rows for `WORDPRESS_RELEASES_DIR`, `WORDPRESS_URL`, `WORDPRESS_USERNAME`, `WORDPRESS_APP_PASSWORD` (all required), matching the existing table's format. Add a short note that `releases-doorserver` and `www.uprough.net` must run on the same host for Phase 2's publish feature to work (the archive transfer is a local file copy, not a network upload) - and a one-line pointer on how to generate a WordPress Application Password (WP Admin → Users → Profile → Application Passwords).

- [ ] **Step 3: Commit**

```bash
git add docker-compose.yml README.md
git commit -m "Document Phase 2 WordPress/publish config in Docker deploy + README"
```

---

## Part B — `makerelease` app additions

### Task 7: Shared types + main-process publish client

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/shared/types.ts`
- Modify: `/Users/spot/Code/makerelease/app/src/main/releases-client.ts`
- Test: `/Users/spot/Code/makerelease/app/test/unit/releases-client.spec.ts`

**Interfaces:**
- Produces:
  - `PublishToWebResult { archiveUrl: string; wordpressPostUrl: string }`
  - `publishToWeb(cfg: ReleasesClientConfig, code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult>` - reads both files from disk and does a real multipart `fetch` POST (Node's built-in `FormData`/`Blob` support, same runtime already used for the plain JSON calls in this file).
  - `MakereleaseApi` gains `publishToWeb(code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult>` and `pickImage(): Promise<string | null>`.

- [ ] **Step 1: Add `PublishToWebResult` to `types.ts`, extend `MakereleaseApi`**

```typescript
export interface PublishToWebResult {
  archiveUrl: string;
  wordpressPostUrl: string;
}

// add to MakereleaseApi:
//   pickImage(): Promise<string | null>;
//   publishToWeb(code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult>;
```

- [ ] **Step 2: Write the failing test for `publishToWeb`**

Add to `test/unit/releases-client.spec.ts`:

```typescript
import * as fs from 'fs';

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
```

- [ ] **Step 3: Run test to verify it fails**

Run: `npx vitest run test/unit/releases-client.spec.ts`
Expected: FAIL - `publishToWeb` not exported.

- [ ] **Step 4: Implement `publishToWeb` in `releases-client.ts`**

```typescript
import * as fs from 'fs/promises';
import * as path from 'path';

export interface PublishToWebResult {
  archiveUrl: string;
  wordpressPostUrl: string;
}

export async function publishToWeb(
  cfg: ReleasesClientConfig,
  code: string,
  archivePath: string,
  imagePath: string,
): Promise<PublishToWebResult> {
  const [archiveBytes, imageBytes] = await Promise.all([fs.readFile(archivePath), fs.readFile(imagePath)]);

  const form = new FormData();
  form.append('archive', new Blob([archiveBytes]), path.basename(archivePath));
  form.append('image', new Blob([imageBytes]), path.basename(imagePath));

  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}/api/releases/${encodeURIComponent(code)}/publish`, {
      method: 'POST',
      headers: { 'X-Release-Key': cfg.writeKey },
      body: form,
    });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError(body.error ?? `releases.uprough.net returned ${response.status}`);
  }
  return body as PublishToWebResult;
}
```

Note: do NOT manually set a `Content-Type` header when sending a `FormData` body - `fetch` sets the correct `multipart/form-data; boundary=...` value itself, and overriding it breaks the multipart parse on the server.

- [ ] **Step 5: Run tests, typecheck**

Run: `npx vitest run test/unit/releases-client.spec.ts && npm run typecheck`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/shared/types.ts src/main/releases-client.ts test/unit/releases-client.spec.ts
git commit -m "Add publishToWeb HTTP client function"
```

---

### Task 8: IPC handlers for image picking + publish

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/main/ipc-handlers.ts`
- Modify: `/Users/spot/Code/makerelease/app/src/preload/preload.ts`
- Test: `/Users/spot/Code/makerelease/app/test/unit/ipc-handlers.spec.ts`

**Interfaces:**
- Consumes: `publishToWeb` (Task 7).
- Produces: two new IPC channels, `pickImage` and `publishToWeb`, registered in `registerIpcHandlers` and exposed via `preload.ts`. `publishToWeb` takes THREE arguments end to end - `(code, archivePath, imagePath)` - `archivePath` is threaded through from the renderer (which already has it from the prior `buildRelease` result, per Task 9), not looked up server-side.

- [ ] **Step 1: Write the failing tests**

Add to `test/unit/ipc-handlers.spec.ts` (reuse this file's existing `vi.mock('electron', ...)` / `vi.mock('../../src/main/releases-client', ...)` setup - add `publishToWeb: vi.fn()` to the existing `releases-client` mock factory):

```typescript
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

it('publishToWeb delegates to the releases-client function with the right args', async () => {
  (releasesClient.publishToWeb as any).mockResolvedValue({ archiveUrl: 'a', wordpressPostUrl: 'b' });
  const result = await handlers.publishToWeb(null, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png');
  expect(result).toEqual({ archiveUrl: 'a', wordpressPostUrl: 'b' });
  expect(releasesClient.publishToWeb).toHaveBeenCalledWith(cfg, 'AAAAA', '/tmp/archive.lha', '/tmp/cover.png');
});
```

(You'll need `electron.dialog` accessible in the test - check how the existing `vi.mock('electron', ...)` factory in this file exposes `dialog`/`ipcMain` and extend it consistently; the mock likely already needs `dialog.showOpenDialog: vi.fn()` for the existing `pickFilesAndDirs` tests to work, so reuse that same mock object.)

- [ ] **Step 2: Run tests to verify they fail**

Run: `npx vitest run test/unit/ipc-handlers.spec.ts`
Expected: FAIL - `handlers.pickImage`/`handlers.publishToWeb` undefined.

- [ ] **Step 3: Add the handlers in `ipc-handlers.ts`**

```typescript
import { publishToWeb as publishToWebClient } from './releases-client';
// (alongside the existing fetchSeed/checkCodeExists/publishRelease import)

// inside registerIpcHandlers, alongside the existing handlers:

ipcMain.handle('pickImage', async (): Promise<string | null> => {
  const result = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'gif'] }],
  });
  if (result.canceled || result.filePaths.length === 0) return null;
  return result.filePaths[0];
});

ipcMain.handle(
  'publishToWeb',
  async (_event, code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult> => {
    return publishToWebClient(releasesConfig, code, archivePath, imagePath);
  },
);
```

- [ ] **Step 4: Wire up `preload.ts`**

```typescript
pickImage: (): Promise<string | null> => ipcRenderer.invoke('pickImage'),
publishToWeb: (code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult> =>
  ipcRenderer.invoke('publishToWeb', code, archivePath, imagePath),
```

Update `MakereleaseApi` in `src/shared/types.ts` (Task 7) to match this exact three-argument signature: `publishToWeb(code: string, archivePath: string, imagePath: string): Promise<PublishToWebResult>`. `archivePath` is threaded through from the renderer, which already has it from the `buildRelease` result (Task 9) - it is not looked up server-side.

- [ ] **Step 5: Run tests, typecheck**

Run: `npx vitest run test/unit/ipc-handlers.spec.ts && npm run typecheck`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/main/ipc-handlers.ts src/preload/preload.ts test/unit/ipc-handlers.spec.ts
git commit -m "Add pickImage and publishToWeb IPC handlers"
```

---

### Task 9: Renderer UI — Publish step

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/renderer/index.html`
- Modify: `/Users/spot/Code/makerelease/app/src/renderer/main.ts`
- Modify: `/Users/spot/Code/makerelease/app/src/renderer/style.css`

**Interfaces:**
- Consumes: `window.api.pickImage()`, `window.api.publishToWeb(code, archivePath, imagePath)` (Task 8).

- [ ] **Step 1: Add the Publish section to `index.html`**

After the existing "Build release" section (`#build-btn`/`#build-status`), add:

```html
<section id="publish-section" hidden>
  <h2>9. Publish to uprough.net</h2>
  <button type="button" id="pick-image-btn">Choose cover image...</button>
  <div id="selected-image" class="status"></div>
  <button type="button" id="publish-btn" disabled>Publish</button>
  <div id="publish-status" class="status"></div>
</section>
```

Hidden by default - only shown after a successful build (Step 3).

- [ ] **Step 2: Style it**

In `style.css`, no new rules strictly needed (existing `.status`/`button` rules cover this), but add:

```css
#publish-section[hidden] { display: none; }
```

- [ ] **Step 3: Wire it up in `main.ts`**

Add references and state:

```typescript
const publishSection = $<HTMLElement>('publish-section');
const pickImageBtn = $<HTMLButtonElement>('pick-image-btn');
const selectedImageDiv = $<HTMLDivElement>('selected-image');
const publishBtn = $<HTMLButtonElement>('publish-btn');
const publishStatus = $<HTMLDivElement>('publish-status');

let lastBuiltCode: string | null = null;
let lastBuiltArchivePath: string | null = null;
let selectedImagePath: string | null = null;
```

In the existing `form.addEventListener('submit', ...)` handler, after a successful `buildRelease` call (inside the `try` block, after setting the success `buildStatus`):

```typescript
lastBuiltCode = code;
lastBuiltArchivePath = result.archivePath;
publishSection.hidden = false;
publishBtn.disabled = !selectedImagePath;
```

New listeners:

```typescript
pickImageBtn.addEventListener('click', async () => {
  const picked = await window.api.pickImage();
  if (picked) {
    selectedImagePath = picked;
    selectedImageDiv.textContent = picked;
    publishBtn.disabled = !lastBuiltArchivePath;
  }
});

publishBtn.addEventListener('click', async () => {
  if (!lastBuiltCode || !selectedImagePath) return;
  publishBtn.disabled = true;
  publishStatus.textContent = 'Publishing...';
  publishStatus.className = 'status';
  try {
    const result = await window.api.publishToWeb(lastBuiltCode, lastBuiltArchivePath, selectedImagePath);
    publishStatus.textContent = `Published: ${result.wordpressPostUrl}`;
    publishStatus.className = 'status ok';
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    if (message.includes('already published')) {
      publishStatus.textContent = `Already published: ${message}`;
      publishStatus.className = 'status warn';
    } else {
      publishStatus.textContent = `Publish failed: ${message}`;
      publishStatus.className = 'status warn';
    }
  } finally {
    publishBtn.disabled = false;
  }
});
```

Note the call site above already uses the three-argument form matching `MakereleaseApi.publishToWeb(code, archivePath, imagePath)` from Tasks 7-8 - `lastBuiltArchivePath` (tracked in this file's own state, set in Step 3 above from the prior `buildRelease` result) is threaded through explicitly rather than looked up server-side.

```typescript
// the publishBtn listener above already calls it correctly:
window.api.publishToWeb(lastBuiltCode, lastBuiltArchivePath, selectedImagePath)
```

- [ ] **Step 4: Rebuild, manually verify**

Run: `npm run build`
Expected: succeeds. Then manually launch the app (`./node_modules/.bin/electron .` from `app/`, pointed at a local test `releases-doorserver` instance per the same pattern used for Phase 1's Task 16), build a real throwaway release, confirm the Publish section appears, pick an image, click Publish, and confirm the request reaches the server (a local test server without real WordPress credentials configured will fail at the WordPress step - that's expected and fine for this manual check; the goal is confirming the UI wiring and the multipart request actually reaches the server's route, not a full real publish, which is Task 10's job).

- [ ] **Step 5: Commit**

```bash
git add src/renderer/index.html src/renderer/main.ts src/renderer/style.css src/preload/preload.ts src/main/ipc-handlers.ts src/shared/types.ts
git commit -m "Add Publish UI: image picker, publish button, status display"
```

---

### Task 10: End-to-end verification

**Files:** none (verification only).

- [ ] **Step 1: Start `releases-doorserver` locally with WordPress config pointed at a real (or realistic staging) WordPress site**

This step genuinely needs real WordPress credentials to verify the full pipeline - if a real staging WordPress instance isn't available in this environment, do as much of this as possible with the WordPress calls mocked at the HTTP level (e.g. a tiny local stub Express server standing in for `wp-json/wp/v2/*` endpoints, returning realistic responses) and clearly report which parts were verified against a real WordPress site vs. a stub, per this project's reachability standard - never claim "verified" for something that was only stubbed.

```bash
touch /tmp/releases-e2e-p2.db
RELEASES_DB=/tmp/releases-e2e-p2.db \
RELEASES_WRITE_KEYS=app:e2e-key \
WORDPRESS_RELEASES_DIR=/tmp/fake-releases-dir \
WORDPRESS_URL=<real or stub URL> \
WORDPRESS_USERNAME=<real or stub username> \
WORDPRESS_APP_PASSWORD=<real or stub app password> \
PORT=3022 npm run dev
```

(`mkdir -p /tmp/fake-releases-dir` first if testing against a stub.)

- [ ] **Step 2: Publish a release's metadata (Phase 1 flow), then publish it (Phase 2 flow)**

```bash
curl -s -X POST http://localhost:3022/api/releases -H 'Content-Type: application/json' -H 'X-Release-Key: e2e-key' \
  -d '{"code":"PTEST","format":"LHA","releaseName":"'"'"'PHASE2 TEST'"'"'","platformTag":"","releaseNotes":"notes","news":"news","dizText":"diz","entryTitle":"test","entryCategory":"Demo","entryCreators":"spot","members":[{"nickname":"spot","role":"leader"}]}'

curl -s -X POST http://localhost:3022/api/releases/PTEST/publish -H 'X-Release-Key: e2e-key' \
  -F 'archive=@/tmp/some-small-test-file;filename=UP-PTEST.LHA' \
  -F 'image=@/tmp/some-small-image.png;filename=cover.png'
```

(Create small placeholder files for `-F` first: `echo test > /tmp/some-small-test-file`, and any small real PNG for the image field.)

Expected: `200` with `{archiveUrl, wordpressPostUrl}`. Confirm the archive landed at `$WORDPRESS_RELEASES_DIR/UP-PTEST.LHA`. If tested against a real WordPress site, visit the returned `wordpressPostUrl` and confirm: title is `#<N>: PHASE2 TEST`, category is `Demos`, the cover image is the featured image, and the body is `notes`.

- [ ] **Step 3: Confirm idempotency**

Repeat the same `POST /api/releases/PTEST/publish` request. Expected: `409` with the same `wordpressPostUrl`, and (if against a real WordPress site) confirm no SECOND post was created.

- [ ] **Step 4: Clean up**

Delete the test WordPress post (if a real site was used), remove the test archive from `$WORDPRESS_RELEASES_DIR`, remove `/tmp/releases-e2e-p2.db` and any other temp files, stop the local server.

- [ ] **Step 5: Report results**

State plainly which parts were verified against a REAL WordPress instance vs. a stub, per this project's reachability standard - "10 of 10 verified against real WordPress" and "7 of 10 verified, 3 against a stub because no staging WordPress was available" are both acceptable outcomes to report; silently treating a stub-only run as full verification is not.
