# releases-doorserver Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `releases-doorserver` (a new, forked-from-`amiexpress-doorserver` metadata API) and switch the `makerelease` Electron app to depend on it instead of local filesystem scanning, with the server assigning release numbers atomically.

**Architecture:** A small Express + better-sqlite3 service mirrors `amiexpress-doorserver`'s config/db/cors/auth conventions but with a static-shared-secret write endpoint (no JWT/login) and no file storage. `makerelease/app`'s `repo-scanner.ts`/`nfo-parser.ts` (local-scan) are deleted and replaced by an HTTP client; `nfo-generator.ts` is simplified since the DB is now the single source of history (no more byte-for-byte legacy-format preservation via `rawBlock` — every history entry, old or new, renders through one uniform formatter).

**Tech Stack:** TypeScript, Express (pinned exact version), better-sqlite3, Jest + supertest (server); existing Electron/TypeScript/Vitest stack (app).

**Spec:** `thoughts/shared/plans/2026-08-24-releases-doorserver-phase1-design.md`

## Global Constraints

- Express is pinned to an **exact** version in `package.json` (no `^`), matching `amiexpress-doorserver`'s own rationale (README) — copy whatever exact version that repo currently pins.
- Server refuses to start if required env vars are missing/invalid — never falls back to a default (same policy as `amiexpress-doorserver/src/config.ts`).
- No JWT, no sessions, no password hashing on the server — a single static secret compared with `crypto.timingSafeEqual`.
- No file storage on the server — download links are always computed client-side as `https://www.uprough.net/releases/UP-<CODE>.<FORMAT>`, never stored.
- The app's write key must never ship inside the Vite/tsc-compiled **renderer** bundle (inspectable via devtools) — it lives only in a main-process-only module.
- Every bug fix / new behavior ships with a regression test that fails before the fix and passes after (existing project convention, both repos).

---

## Part A — `releases-doorserver` (new repo)

### Task 1: Scaffold the repo

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/package.json`
- Create: `/Users/spot/Code/releases-doorserver/tsconfig.json`
- Create: `/Users/spot/Code/releases-doorserver/jest.config.ts`
- Create: `/Users/spot/Code/releases-doorserver/.gitignore`

**Interfaces:**
- Produces: an installable, typecheckable, testable (empty) TypeScript project other tasks add `src/*.ts` and `tests/*.test.ts` files into.

- [ ] **Step 1: Check the exact Express version pinned by the reference project**

Run: `grep '"express"' /Users/spot/Code/amiexpress-doorserver/package.json`
Expected output: a line like `"express": "5.1.0",` — record this exact string, it is used verbatim in Step 2.

- [ ] **Step 2: Create the directory and `package.json`**

```bash
mkdir -p /Users/spot/Code/releases-doorserver/src /Users/spot/Code/releases-doorserver/tests
cd /Users/spot/Code/releases-doorserver
git init
```

Write `package.json` (replace `<EXPRESS_VERSION>` with the exact string from Step 1, e.g. `5.1.0`):

```json
{
  "name": "releases-doorserver",
  "version": "0.1.0",
  "private": true,
  "type": "commonjs",
  "scripts": {
    "build": "tsc -p tsconfig.json && cp src/schema.sql dist/src/schema.sql",
    "start": "node dist/src/index.js",
    "dev": "tsx watch src/index.ts",
    "typecheck": "tsc --noEmit",
    "test": "jest --config jest.config.ts",
    "test:ci": "jest --config jest.config.ts --ci"
  },
  "dependencies": {
    "better-sqlite3": "^11.10.0",
    "express": "<EXPRESS_VERSION>"
  },
  "devDependencies": {
    "@types/better-sqlite3": "^7.6.11",
    "@types/express": "^5.0.0",
    "@types/jest": "^29.5.12",
    "@types/node": "^24.10.1",
    "@types/supertest": "^6.0.2",
    "jest": "^29.7.0",
    "supertest": "^7.0.0",
    "ts-jest": "^29.1.2",
    "tsx": "^4.19.2",
    "typescript": "^5.9.3"
  }
}
```

- [ ] **Step 3: Create `tsconfig.json`**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "CommonJS",
    "moduleResolution": "Node",
    "lib": ["ES2022"],
    "strict": true,
    "esModuleInterop": true,
    "forceConsistentCasingInFileNames": true,
    "skipLibCheck": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "outDir": "dist",
    "rootDir": "."
  },
  "include": ["src/**/*.ts", "tests/**/*.ts"]
}
```

- [ ] **Step 4: Create `jest.config.ts`**

```typescript
import type { Config } from 'jest';

const config: Config = {
  preset: 'ts-jest',
  testEnvironment: 'node',
  testMatch: ['**/tests/**/*.test.ts'],
};

export default config;
```

- [ ] **Step 5: Create `.gitignore`**

```
node_modules/
dist/
*.db
.env
```

- [ ] **Step 6: Install dependencies**

Run: `cd /Users/spot/Code/releases-doorserver && npm install`
Expected: exits 0, `node_modules/` created.

- [ ] **Step 7: Verify empty typecheck and test pass**

Run: `npm run typecheck && npm test`
Expected: typecheck passes (no `.ts` files yet, trivially), `npm test` reports "No tests found" (expected — no test files yet) without erroring.

- [ ] **Step 8: Commit**

```bash
git add package.json tsconfig.json jest.config.ts .gitignore
git commit -m "Scaffold releases-doorserver project"
```

---

### Task 2: Config module

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/config.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/config.test.ts`

**Interfaces:**
- Produces: `ServerConfig` interface `{ dbPath: string; port: number; writeKeys: { label: string; key: string }[] }`, `loadConfig(env?: NodeJS.ProcessEnv): ServerConfig`, `ConfigError` class.

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/config.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { loadConfig, ConfigError } from '../src/config';

describe('loadConfig', () => {
  let tmpDb: string;

  beforeEach(() => {
    tmpDb = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-cfg-')), 'releases.db');
    fs.writeFileSync(tmpDb, '');
  });

  it('throws when RELEASES_DB is not set', () => {
    expect(() => loadConfig({})).toThrow(ConfigError);
  });

  it('throws when RELEASES_DB points at a nonexistent path', () => {
    expect(() => loadConfig({ RELEASES_DB: '/nonexistent/releases.db', RELEASES_WRITE_KEYS: 'app:secret' })).toThrow(
      ConfigError,
    );
  });

  it('throws when RELEASES_WRITE_KEYS is not set', () => {
    expect(() => loadConfig({ RELEASES_DB: tmpDb })).toThrow(ConfigError);
  });

  it('parses a single label:key pair', () => {
    const cfg = loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:supersecret' });
    expect(cfg.writeKeys).toEqual([{ label: 'app', key: 'supersecret' }]);
  });

  it('parses multiple comma-separated label:key pairs', () => {
    const cfg = loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:one,spot:two' });
    expect(cfg.writeKeys).toEqual([
      { label: 'app', key: 'one' },
      { label: 'spot', key: 'two' },
    ]);
  });

  it('throws on a malformed write-key entry with no colon', () => {
    expect(() => loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'notapair' })).toThrow(ConfigError);
  });

  it('defaults PORT to 3020 when unset', () => {
    const cfg = loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:secret' });
    expect(cfg.port).toBe(3020);
  });

  it('uses PORT when set to a valid integer', () => {
    const cfg = loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:secret', PORT: '4000' });
    expect(cfg.port).toBe(4000);
  });

  it('throws when PORT is not a positive integer', () => {
    expect(() => loadConfig({ RELEASES_DB: tmpDb, RELEASES_WRITE_KEYS: 'app:secret', PORT: 'not-a-number' })).toThrow(
      ConfigError,
    );
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- config.test.ts`
Expected: FAIL — `Cannot find module '../src/config'`.

- [ ] **Step 3: Write the implementation**

```typescript
// src/config.ts
import * as fs from 'fs';

export class ConfigError extends Error {}

export interface WriteKey {
  label: string;
  key: string;
}

export interface ServerConfig {
  dbPath: string;
  port: number;
  writeKeys: WriteKey[];
}

const DEFAULT_PORT = 3020;

function required(env: NodeJS.ProcessEnv, name: string): string {
  const value = env[name];
  if (!value) {
    throw new ConfigError(`${name} is not set; the server refuses to start without it`);
  }
  return value;
}

function mustExist(name: string, value: string): string {
  if (!fs.existsSync(value)) {
    throw new ConfigError(`${name} points at ${value}, which does not exist`);
  }
  return value;
}

function parseWriteKeys(raw: string): WriteKey[] {
  return raw
    .split(',')
    .map((pair) => pair.trim())
    .filter((pair) => pair.length > 0)
    .map((pair) => {
      const idx = pair.indexOf(':');
      if (idx <= 0 || idx === pair.length - 1) {
        throw new ConfigError(`RELEASES_WRITE_KEYS entry "${pair}" is not <label>:<key>`);
      }
      return { label: pair.slice(0, idx), key: pair.slice(idx + 1).trim() };
    });
}

export function loadConfig(env: NodeJS.ProcessEnv = process.env): ServerConfig {
  const dbPath = mustExist('RELEASES_DB', required(env, 'RELEASES_DB'));
  const writeKeys = parseWriteKeys(required(env, 'RELEASES_WRITE_KEYS'));
  const port = env.PORT ? Number(env.PORT) : DEFAULT_PORT;
  if (!Number.isInteger(port) || port <= 0) {
    throw new ConfigError(`PORT is "${env.PORT}", which is not a usable port number`);
  }
  return { dbPath, port, writeKeys };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npm test -- config.test.ts`
Expected: PASS, all 9 tests.

- [ ] **Step 5: Typecheck**

Run: `npm run typecheck`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/config.ts tests/config.test.ts
git commit -m "Add config loading with fail-fast validation"
```

---

### Task 3: Database + schema

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/schema.sql`
- Create: `/Users/spot/Code/releases-doorserver/src/db.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/db.test.ts`

**Interfaces:**
- Consumes: `ServerConfig` (Task 2).
- Produces: `openDb(cfg: ServerConfig): Database.Database`, `applySchema(db: Database.Database): void`.

- [ ] **Step 1: Write `schema.sql`**

```sql
CREATE TABLE IF NOT EXISTS releases (
  id               INTEGER PRIMARY KEY AUTOINCREMENT,
  code             TEXT NOT NULL UNIQUE,
  release_number   INTEGER NOT NULL UNIQUE,
  format           TEXT NOT NULL,
  release_name     TEXT NOT NULL,
  platform_tag     TEXT,
  release_notes    TEXT NOT NULL,
  news             TEXT NOT NULL,
  diz_text         TEXT NOT NULL,
  entry_title      TEXT NOT NULL,
  entry_category   TEXT NOT NULL,
  entry_creators   TEXT NOT NULL,
  created_at       INTEGER DEFAULT (strftime('%s','now'))
);
CREATE INDEX IF NOT EXISTS idx_releases_number ON releases(release_number DESC);

CREATE TABLE IF NOT EXISTS members (
  nickname   TEXT PRIMARY KEY,
  role       TEXT NOT NULL,
  updated_at INTEGER DEFAULT (strftime('%s','now'))
);

CREATE TABLE IF NOT EXISTS meta (
  key   TEXT PRIMARY KEY,
  value INTEGER NOT NULL
);
INSERT OR IGNORE INTO meta (key, value) VALUES ('revision', 0);
```

- [ ] **Step 2: Write the failing test**

```typescript
// tests/db.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { openDb, applySchema } from '../src/db';
import type { ServerConfig } from '../src/config';

function tmpConfig(): ServerConfig {
  const dbPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-db-')), 'releases.db');
  return { dbPath, port: 3020, writeKeys: [{ label: 'test', key: 'x' }] };
}

describe('openDb + applySchema', () => {
  it('creates all three tables', () => {
    const cfg = tmpConfig();
    const db = openDb(cfg);
    applySchema(db);
    const tables = db
      .prepare("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
      .all()
      .map((r: any) => r.name);
    expect(tables).toEqual(expect.arrayContaining(['releases', 'members', 'meta']));
    db.close();
  });

  it('seeds meta.revision to 0', () => {
    const cfg = tmpConfig();
    const db = openDb(cfg);
    applySchema(db);
    const row = db.prepare('SELECT value FROM meta WHERE key = ?').get('revision') as { value: number };
    expect(row.value).toBe(0);
    db.close();
  });

  it('applySchema is idempotent (running twice does not throw or reset revision)', () => {
    const cfg = tmpConfig();
    const db = openDb(cfg);
    applySchema(db);
    db.prepare('UPDATE meta SET value = 5 WHERE key = ?').run('revision');
    applySchema(db);
    const row = db.prepare('SELECT value FROM meta WHERE key = ?').get('revision') as { value: number };
    expect(row.value).toBe(5);
    db.close();
  });
});
```

- [ ] **Step 3: Run test to verify it fails**

Run: `npm test -- db.test.ts`
Expected: FAIL — `Cannot find module '../src/db'`.

- [ ] **Step 4: Write `db.ts`**

```typescript
// src/db.ts
import Database from 'better-sqlite3';
import * as fs from 'fs';
import * as path from 'path';
import type { ServerConfig } from './config';

export function openDb(cfg: ServerConfig, opts?: { readonly?: boolean }): Database.Database {
  return new Database(cfg.dbPath, { readonly: opts?.readonly ?? false });
}

export function applySchema(db: Database.Database): void {
  const schemaPath = path.join(__dirname, 'schema.sql');
  db.exec(fs.readFileSync(schemaPath, 'utf-8'));
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `npm test -- db.test.ts`
Expected: PASS, all 3 tests. (Note: this reads `src/schema.sql` via `__dirname`, which works under `ts-jest` running from `src/` directly — no build step needed for tests.)

- [ ] **Step 6: Commit**

```bash
git add src/schema.sql src/db.ts tests/db.test.ts
git commit -m "Add SQLite schema and db open/migrate helpers"
```

---

### Task 4: Catalog logic (the atomic release-number assignment)

This is the highest-risk task — it's the whole reason the server exists (fixing the concurrent-build race). Test the concurrency behavior directly, not just the happy path.

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/catalog.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/catalog.test.ts`

**Interfaces:**
- Consumes: `Database.Database` (Task 3).
- Produces:
  - `interface MemberRow { nickname: string; role: string }`
  - `interface HistoryRow { releaseNumber: number; filenames: string; title: string; category: string; creators: string }`
  - `interface NewReleaseInput { code: string; format: string; releaseName: string; platformTag: string | null; releaseNotes: string; news: string; dizText: string; entryTitle: string; entryCategory: string; entryCreators: string; members: MemberRow[] }`
  - `getSeed(db): { members: MemberRow[]; nextReleaseNumber: number; history: HistoryRow[]; currentDizText: string | null; revision: number }`
  - `getManifest(db): { members: MemberRow[]; history: HistoryRow[]; revision: number }`
  - `codeExists(db, code: string): boolean`
  - `insertRelease(db, input: NewReleaseInput): { releaseNumber: number; revision: number; history: HistoryRow[] }` — throws `CodeConflictError` if `code` already exists.
  - `getRevision(db): number`
  - `class CodeConflictError extends Error`

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/catalog.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import Database from 'better-sqlite3';
import { openDb, applySchema } from '../src/db';
import { getSeed, getManifest, codeExists, insertRelease, CodeConflictError } from '../src/catalog';
import type { ServerConfig } from '../src/config';

function freshDb(): Database.Database {
  const dbPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-cat-')), 'releases.db');
  const cfg: ServerConfig = { dbPath, port: 3020, writeKeys: [] };
  const db = openDb(cfg);
  applySchema(db);
  return db;
}

const baseInput = (code: string) => ({
  code,
  format: 'LHA',
  releaseName: `'${code}'`,
  platformTag: 'A1200',
  releaseNotes: 'notes',
  news: 'news',
  dizText: 'diz art',
  entryTitle: code.toLowerCase(),
  entryCategory: 'Demo',
  entryCreators: 'spot',
  members: [{ nickname: 'spot', role: 'leader' }],
});

describe('getSeed on an empty catalog', () => {
  it('returns nextReleaseNumber 1, empty history, no diz', () => {
    const db = freshDb();
    const seed = getSeed(db);
    expect(seed.nextReleaseNumber).toBe(1);
    expect(seed.history).toEqual([]);
    expect(seed.currentDizText).toBeNull();
    expect(seed.members).toEqual([]);
    db.close();
  });
});

describe('insertRelease', () => {
  it('assigns release_number 1 to the first release', () => {
    const db = freshDb();
    const result = insertRelease(db, baseInput('AAAAA'));
    expect(result.releaseNumber).toBe(1);
    db.close();
  });

  it('assigns sequential numbers to successive releases', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    const second = insertRelease(db, baseInput('BBBBB'));
    expect(second.releaseNumber).toBe(2);
    db.close();
  });

  it('rejects a duplicate code with CodeConflictError', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    expect(() => insertRelease(db, baseInput('AAAAA'))).toThrow(CodeConflictError);
    db.close();
  });

  it('replaces the members table with the submitted roster', () => {
    const db = freshDb();
    insertRelease(db, { ...baseInput('AAAAA'), members: [{ nickname: 'spot', role: 'leader' }] });
    insertRelease(db, {
      ...baseInput('BBBBB'),
      members: [
        { nickname: 'spot', role: 'leader' },
        { nickname: 'qwan', role: 'co-leader' },
      ],
    });
    const seed = getSeed(db);
    expect(seed.members).toEqual([
      { nickname: 'spot', role: 'leader' },
      { nickname: 'qwan', role: 'co-leader' },
    ]);
    db.close();
  });

  it('increments revision on every insert', () => {
    const db = freshDb();
    const before = getSeed(db).revision;
    insertRelease(db, baseInput('AAAAA'));
    const after = getSeed(db).revision;
    expect(after).toBe(before + 1);
    db.close();
  });

  it('returns fresh history including the just-inserted release first', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    const result = insertRelease(db, baseInput('BBBBB'));
    expect(result.history[0].releaseNumber).toBe(2);
    expect(result.history[0].filenames).toBe('up-bbbbb.lha');
    expect(result.history[1].releaseNumber).toBe(1);
    db.close();
  });

  it('assigns strictly increasing, non-colliding numbers under concurrent inserts', async () => {
    const db = freshDb();
    const codes = Array.from({ length: 20 }, (_, i) => `T${String(i).padStart(4, '0')}`);
    const results = await Promise.all(codes.map((code) => Promise.resolve(insertRelease(db, baseInput(code)))));
    const numbers = results.map((r) => r.releaseNumber).sort((a, b) => a - b);
    expect(numbers).toEqual(Array.from({ length: 20 }, (_, i) => i + 1));
    expect(new Set(numbers).size).toBe(20);
    db.close();
  });
});

describe('getSeed after inserts', () => {
  it('reflects the most recent release as currentDizText', () => {
    const db = freshDb();
    insertRelease(db, { ...baseInput('AAAAA'), dizText: 'first diz' });
    insertRelease(db, { ...baseInput('BBBBB'), dizText: 'second diz' });
    expect(getSeed(db).currentDizText).toBe('second diz');
    db.close();
  });

  it('reports the correct nextReleaseNumber', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    expect(getSeed(db).nextReleaseNumber).toBe(2);
    db.close();
  });
});

describe('codeExists', () => {
  it('is false for an unused code and true after insert', () => {
    const db = freshDb();
    expect(codeExists(db, 'AAAAA')).toBe(false);
    insertRelease(db, baseInput('AAAAA'));
    expect(codeExists(db, 'AAAAA')).toBe(true);
    db.close();
  });
});

describe('getManifest', () => {
  it('returns all releases and members', () => {
    const db = freshDb();
    insertRelease(db, baseInput('AAAAA'));
    const manifest = getManifest(db);
    expect(manifest.history.length).toBe(1);
    expect(manifest.members.length).toBe(1);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- catalog.test.ts`
Expected: FAIL — `Cannot find module '../src/catalog'`.

- [ ] **Step 3: Write the implementation**

`better-sqlite3` is synchronous and every connection serializes writes through SQLite's own file lock, so wrapping the read-max/insert/replace-members/bump-revision sequence in a single `db.transaction(...)` call is sufficient for atomicity — no manual locking needed. This is what makes the concurrency test in Step 1 pass: `better-sqlite3` transactions block until the previous one commits.

```typescript
// src/catalog.ts
import type Database from 'better-sqlite3';

export class CodeConflictError extends Error {}

export interface MemberRow {
  nickname: string;
  role: string;
}

export interface HistoryRow {
  releaseNumber: number;
  filenames: string;
  title: string;
  category: string;
  creators: string;
}

export interface NewReleaseInput {
  code: string;
  format: string;
  releaseName: string;
  platformTag: string | null;
  releaseNotes: string;
  news: string;
  dizText: string;
  entryTitle: string;
  entryCategory: string;
  entryCreators: string;
  members: MemberRow[];
}

export interface SeedData {
  members: MemberRow[];
  nextReleaseNumber: number;
  history: HistoryRow[];
  currentDizText: string | null;
  revision: number;
}

function filenamesFor(code: string, format: string): string {
  return `up-${code.toLowerCase()}.${format.toLowerCase()}`;
}

function readHistory(db: Database.Database): HistoryRow[] {
  const rows = db
    .prepare(
      `SELECT release_number as releaseNumber, code, format, entry_title as title,
              entry_category as category, entry_creators as creators
       FROM releases ORDER BY release_number DESC`,
    )
    .all() as Array<{ releaseNumber: number; code: string; format: string; title: string; category: string; creators: string }>;
  return rows.map((r) => ({
    releaseNumber: r.releaseNumber,
    filenames: filenamesFor(r.code, r.format),
    title: r.title,
    category: r.category,
    creators: r.creators,
  }));
}

function readMembers(db: Database.Database): MemberRow[] {
  return db.prepare('SELECT nickname, role FROM members ORDER BY nickname ASC').all() as MemberRow[];
}

export function getRevision(db: Database.Database): number {
  const row = db.prepare('SELECT value FROM meta WHERE key = ?').get('revision') as { value: number };
  return row.value;
}

export function getSeed(db: Database.Database): SeedData {
  const history = readHistory(db);
  const latestDiz = db.prepare('SELECT diz_text as dizText FROM releases ORDER BY release_number DESC LIMIT 1').get() as
    | { dizText: string }
    | undefined;
  return {
    members: readMembers(db),
    nextReleaseNumber: (history[0]?.releaseNumber ?? 0) + 1,
    history,
    currentDizText: latestDiz?.dizText ?? null,
    revision: getRevision(db),
  };
}

export function getManifest(db: Database.Database): { members: MemberRow[]; history: HistoryRow[]; revision: number } {
  return { members: readMembers(db), history: readHistory(db), revision: getRevision(db) };
}

export function codeExists(db: Database.Database, code: string): boolean {
  const row = db.prepare('SELECT 1 FROM releases WHERE code = ?').get(code);
  return row !== undefined;
}

export function insertRelease(
  db: Database.Database,
  input: NewReleaseInput,
): { releaseNumber: number; revision: number; history: HistoryRow[] } {
  const run = db.transaction(() => {
    if (codeExists(db, input.code)) {
      throw new CodeConflictError(`code ${input.code} already exists`);
    }
    const maxRow = db.prepare('SELECT MAX(release_number) as maxNumber FROM releases').get() as {
      maxNumber: number | null;
    };
    const releaseNumber = (maxRow.maxNumber ?? 0) + 1;

    db.prepare(
      `INSERT INTO releases
        (code, release_number, format, release_name, platform_tag, release_notes, news,
         diz_text, entry_title, entry_category, entry_creators)
       VALUES (@code, @releaseNumber, @format, @releaseName, @platformTag, @releaseNotes, @news,
               @dizText, @entryTitle, @entryCategory, @entryCreators)`,
    ).run({ ...input, releaseNumber });

    db.prepare('DELETE FROM members').run();
    const insertMember = db.prepare('INSERT INTO members (nickname, role) VALUES (?, ?)');
    for (const m of input.members) insertMember.run(m.nickname, m.role);

    db.prepare('UPDATE meta SET value = value + 1 WHERE key = ?').run('revision');

    return releaseNumber;
  });

  const releaseNumber = run();
  return { releaseNumber, revision: getRevision(db), history: readHistory(db) };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npm test -- catalog.test.ts`
Expected: PASS, all tests including the 20-way concurrent insert test.

- [ ] **Step 5: Typecheck**

Run: `npm run typecheck`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/catalog.ts tests/catalog.test.ts
git commit -m "Add catalog logic with atomic release-number assignment"
```

---

### Task 5: Auth middleware

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/auth.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/auth.test.ts`

**Interfaces:**
- Consumes: `ServerConfig.writeKeys` (Task 2).
- Produces: `requireWriteKey(cfg: ServerConfig): express.RequestHandler` — Express middleware.

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/auth.test.ts
import express from 'express';
import request from 'supertest';
import { requireWriteKey } from '../src/auth';
import type { ServerConfig } from '../src/config';

function appWith(cfg: ServerConfig) {
  const app = express();
  app.get('/protected', requireWriteKey(cfg), (_req, res) => res.status(200).json({ ok: true }));
  return app;
}

const cfg: ServerConfig = { dbPath: ':memory:', port: 3020, writeKeys: [{ label: 'app', key: 'supersecret' }] };

describe('requireWriteKey', () => {
  it('rejects a request with no key header with 401', async () => {
    const res = await request(appWith(cfg)).get('/protected');
    expect(res.status).toBe(401);
  });

  it('rejects a request with the wrong key with 401', async () => {
    const res = await request(appWith(cfg)).get('/protected').set('X-Release-Key', 'wrong');
    expect(res.status).toBe(401);
  });

  it('accepts a request with the correct key', async () => {
    const res = await request(appWith(cfg)).get('/protected').set('X-Release-Key', 'supersecret');
    expect(res.status).toBe(200);
  });

  it('accepts any one of multiple configured keys', async () => {
    const multi: ServerConfig = {
      ...cfg,
      writeKeys: [
        { label: 'app', key: 'one' },
        { label: 'spot', key: 'two' },
      ],
    };
    const res = await request(appWith(multi)).get('/protected').set('X-Release-Key', 'two');
    expect(res.status).toBe(200);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- auth.test.ts`
Expected: FAIL — `Cannot find module '../src/auth'`.

- [ ] **Step 3: Write the implementation**

```typescript
// src/auth.ts
import * as crypto from 'crypto';
import type { NextFunction, Request, RequestHandler, Response } from 'express';
import type { ServerConfig } from './config';

function constantTimeEquals(a: string, b: string): boolean {
  const bufA = Buffer.from(a);
  const bufB = Buffer.from(b);
  if (bufA.length !== bufB.length) return false;
  return crypto.timingSafeEqual(bufA, bufB);
}

export function requireWriteKey(cfg: ServerConfig): RequestHandler {
  return (req: Request, res: Response, next: NextFunction): void => {
    const provided = req.get('X-Release-Key');
    if (!provided || !cfg.writeKeys.some((k) => constantTimeEquals(k.key, provided))) {
      res.status(401).json({ error: 'missing or invalid X-Release-Key' });
      return;
    }
    next();
  };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npm test -- auth.test.ts`
Expected: PASS, all 4 tests.

- [ ] **Step 5: Commit**

```bash
git add src/auth.ts tests/auth.test.ts
git commit -m "Add static shared-key write auth middleware"
```

---

### Task 6: CORS middleware

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/cors.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/cors.test.ts`

**Interfaces:**
- Produces: `releasesRepoCors(): express.RequestHandler`.

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/cors.test.ts
import express from 'express';
import request from 'supertest';
import { releasesRepoCors } from '../src/cors';

function testApp() {
  const app = express();
  app.use(releasesRepoCors());
  app.get('/x', (_req, res) => res.json({ ok: true }));
  return app;
}

describe('releasesRepoCors', () => {
  it('sets Access-Control-Allow-Origin: * on a GET', async () => {
    const res = await request(testApp()).get('/x');
    expect(res.headers['access-control-allow-origin']).toBe('*');
  });

  it('sets Cross-Origin-Resource-Policy: cross-origin', async () => {
    const res = await request(testApp()).get('/x');
    expect(res.headers['cross-origin-resource-policy']).toBe('cross-origin');
  });

  it('never sets Access-Control-Allow-Credentials', async () => {
    const res = await request(testApp()).get('/x');
    expect(res.headers['access-control-allow-credentials']).toBeUndefined();
  });

  it('answers OPTIONS with an empty 204', async () => {
    const res = await request(testApp()).options('/x');
    expect(res.status).toBe(204);
    expect(res.text).toBe('');
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- cors.test.ts`
Expected: FAIL — `Cannot find module '../src/cors'`.

- [ ] **Step 3: Write the implementation** (adapted from `amiexpress-doorserver/src/cors.ts`, simplified — no `Access-Control-Expose-Headers` list needed since Phase 1 has no download/checksum headers)

```typescript
// src/cors.ts
import type { NextFunction, Request, RequestHandler, Response } from 'express';

export function releasesRepoCors(): RequestHandler {
  return (req: Request, res: Response, next: NextFunction): void => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');

    if (req.method === 'OPTIONS') {
      res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
      res.setHeader('Access-Control-Allow-Headers', 'Content-Type, X-Release-Key');
      res.setHeader('Access-Control-Max-Age', '86400');
      res.status(204).end();
      return;
    }

    next();
  };
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npm test -- cors.test.ts`
Expected: PASS, all 4 tests.

- [ ] **Step 5: Commit**

```bash
git add src/cors.ts tests/cors.test.ts
git commit -m "Add CORS middleware for public cross-origin reads"
```

---

### Task 7: Routes

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/routes.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/routes.test.ts`

**Interfaces:**
- Consumes: `ServerConfig` (Task 2), `openDb`/`applySchema` (Task 3), `getSeed`/`getManifest`/`codeExists`/`insertRelease`/`CodeConflictError` (Task 4), `requireWriteKey` (Task 5).
- Produces: `createRouter(cfg: ServerConfig): express.Router` mounted at `/api/releases`.

- [ ] **Step 1: Write the failing tests**

```typescript
// tests/routes.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import express from 'express';
import request from 'supertest';
import { createRouter } from '../src/routes';
import { openDb, applySchema } from '../src/db';
import type { ServerConfig } from '../src/config';

function testAppAndConfig() {
  const dbPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-routes-')), 'releases.db');
  const cfg: ServerConfig = { dbPath, port: 3020, writeKeys: [{ label: 'app', key: 'secret' }] };
  const db = openDb(cfg);
  applySchema(db);
  db.close();
  const app = express();
  app.use(express.json());
  app.use('/api/releases', createRouter(cfg));
  return app;
}

const validBody = (code: string) => ({
  code,
  format: 'LHA',
  releaseName: `'${code}'`,
  platformTag: 'A1200',
  releaseNotes: 'notes',
  news: 'news',
  dizText: 'diz',
  entryTitle: code.toLowerCase(),
  entryCategory: 'Demo',
  entryCreators: 'spot',
  members: [{ nickname: 'spot', role: 'leader' }],
});

describe('GET /api/releases/seed', () => {
  it('returns 200 with nextReleaseNumber 1 on an empty catalog, no auth required', async () => {
    const res = await request(testAppAndConfig()).get('/api/releases/seed');
    expect(res.status).toBe(200);
    expect(res.body.nextReleaseNumber).toBe(1);
  });
});

describe('GET /api/releases/health', () => {
  it('returns status ok and releases count', async () => {
    const res = await request(testAppAndConfig()).get('/api/releases/health');
    expect(res.status).toBe(200);
    expect(res.body).toEqual({ status: 'ok', revision: 0, releases: 0 });
  });
});

describe('GET /api/releases/:code/exists', () => {
  it('returns false for an unused code', async () => {
    const res = await request(testAppAndConfig()).get('/api/releases/ZZZZZ/exists');
    expect(res.body).toEqual({ exists: false });
  });
});

describe('POST /api/releases', () => {
  it('rejects a request with no X-Release-Key with 401', async () => {
    const res = await request(testAppAndConfig()).post('/api/releases').send(validBody('AAAAA'));
    expect(res.status).toBe(401);
  });

  it('accepts a valid authenticated request and returns releaseNumber 1', async () => {
    const res = await request(testAppAndConfig())
      .post('/api/releases')
      .set('X-Release-Key', 'secret')
      .send(validBody('AAAAA'));
    expect(res.status).toBe(200);
    expect(res.body.releaseNumber).toBe(1);
    expect(res.body.history[0].filenames).toBe('up-aaaaa.lha');
  });

  it('rejects a duplicate code with 400', async () => {
    const app = testAppAndConfig();
    await request(app).post('/api/releases').set('X-Release-Key', 'secret').send(validBody('AAAAA'));
    const res = await request(app).post('/api/releases').set('X-Release-Key', 'secret').send(validBody('AAAAA'));
    expect(res.status).toBe(400);
  });

  it('rejects a body missing a required field with 400', async () => {
    const { code, ...withoutCode } = validBody('AAAAA');
    const res = await request(testAppAndConfig())
      .post('/api/releases')
      .set('X-Release-Key', 'secret')
      .send(withoutCode);
    expect(res.status).toBe(400);
  });

  it('a code becomes reported as existing after insert', async () => {
    const app = testAppAndConfig();
    await request(app).post('/api/releases').set('X-Release-Key', 'secret').send(validBody('AAAAA'));
    const res = await request(app).get('/api/releases/AAAAA/exists');
    expect(res.body).toEqual({ exists: true });
  });
});

describe('GET /api/releases/manifest', () => {
  it('reflects an inserted release', async () => {
    const app = testAppAndConfig();
    await request(app).post('/api/releases').set('X-Release-Key', 'secret').send(validBody('AAAAA'));
    const res = await request(app).get('/api/releases/manifest');
    expect(res.body.history.length).toBe(1);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm test -- routes.test.ts`
Expected: FAIL — `Cannot find module '../src/routes'`.

- [ ] **Step 3: Write the implementation**

```typescript
// src/routes.ts
import { Router, type Request, type Response } from 'express';
import { openDb } from './db';
import { getSeed, getManifest, codeExists, insertRelease, CodeConflictError, type NewReleaseInput } from './catalog';
import { requireWriteKey } from './auth';
import type { ServerConfig } from './config';

const REQUIRED_FIELDS: (keyof NewReleaseInput)[] = [
  'code',
  'format',
  'releaseName',
  'releaseNotes',
  'news',
  'dizText',
  'entryTitle',
  'entryCategory',
  'entryCreators',
  'members',
];

function validateBody(body: unknown): { ok: true; input: NewReleaseInput } | { ok: false; error: string } {
  if (typeof body !== 'object' || body === null) return { ok: false, error: 'body must be a JSON object' };
  const b = body as Record<string, unknown>;
  for (const field of REQUIRED_FIELDS) {
    if (b[field] === undefined || b[field] === null || b[field] === '') {
      return { ok: false, error: `missing required field: ${field}` };
    }
  }
  if (!Array.isArray(b.members)) return { ok: false, error: 'members must be an array' };
  return {
    ok: true,
    input: {
      code: String(b.code),
      format: String(b.format),
      releaseName: String(b.releaseName),
      platformTag: b.platformTag ? String(b.platformTag) : null,
      releaseNotes: String(b.releaseNotes),
      news: String(b.news),
      dizText: String(b.dizText),
      entryTitle: String(b.entryTitle),
      entryCategory: String(b.entryCategory),
      entryCreators: String(b.entryCreators),
      members: b.members as { nickname: string; role: string }[],
    },
  };
}

export function createRouter(cfg: ServerConfig): Router {
  const router = Router();

  router.get('/seed', (_req: Request, res: Response) => {
    const db = openDb(cfg, { readonly: true });
    try {
      res.json(getSeed(db));
    } finally {
      db.close();
    }
  });

  router.get('/manifest', (_req: Request, res: Response) => {
    const db = openDb(cfg, { readonly: true });
    try {
      res.json(getManifest(db));
    } finally {
      db.close();
    }
  });

  router.get('/health', (_req: Request, res: Response) => {
    const db = openDb(cfg, { readonly: true });
    try {
      const manifest = getManifest(db);
      res.json({ status: 'ok', revision: manifest.revision, releases: manifest.history.length });
    } finally {
      db.close();
    }
  });

  router.get('/:code/exists', (req: Request, res: Response) => {
    const db = openDb(cfg, { readonly: true });
    try {
      res.json({ exists: codeExists(db, req.params.code.toUpperCase()) });
    } finally {
      db.close();
    }
  });

  router.post('/', requireWriteKey(cfg), (req: Request, res: Response) => {
    const validated = validateBody(req.body);
    if (!validated.ok) {
      res.status(400).json({ error: validated.error });
      return;
    }
    const db = openDb(cfg);
    try {
      const input = { ...validated.input, code: validated.input.code.toUpperCase() };
      const result = insertRelease(db, input);
      res.json(result);
    } catch (err) {
      if (err instanceof CodeConflictError) {
        res.status(400).json({ error: err.message });
        return;
      }
      throw err;
    } finally {
      db.close();
    }
  });

  return router;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npm test -- routes.test.ts`
Expected: PASS, all tests.

- [ ] **Step 5: Typecheck**

Run: `npm run typecheck`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/routes.ts tests/routes.test.ts
git commit -m "Add seed/manifest/health/exists/POST-release routes"
```

---

### Task 8: App wiring + entrypoint

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/src/app.ts`
- Create: `/Users/spot/Code/releases-doorserver/src/index.ts`
- Test: `/Users/spot/Code/releases-doorserver/tests/startup.test.ts`

**Interfaces:**
- Consumes: everything from Tasks 2-7.
- Produces: `createApp(cfg: ServerConfig): express.Express`; `index.ts` boots the process (not imported by tests except via `require.main === module` guard, same pattern as the reference project).

- [ ] **Step 1: Write the failing test**

```typescript
// tests/startup.test.ts
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import request from 'supertest';
import { createApp } from '../src/app';
import { openDb, applySchema } from '../src/db';
import type { ServerConfig } from '../src/config';

describe('createApp', () => {
  it('serves /api/releases/health', async () => {
    const dbPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-app-')), 'releases.db');
    const cfg: ServerConfig = { dbPath, port: 3020, writeKeys: [{ label: 'app', key: 'secret' }] };
    const db = openDb(cfg);
    applySchema(db);
    db.close();

    const app = createApp(cfg);
    const res = await request(app).get('/api/releases/health');
    expect(res.status).toBe(200);
    expect(res.body.status).toBe('ok');
  });

  it('CORS headers are present on /api/releases/seed', async () => {
    const dbPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rds-app-')), 'releases.db');
    const cfg: ServerConfig = { dbPath, port: 3020, writeKeys: [{ label: 'app', key: 'secret' }] };
    const db = openDb(cfg);
    applySchema(db);
    db.close();

    const app = createApp(cfg);
    const res = await request(app).get('/api/releases/seed');
    expect(res.headers['access-control-allow-origin']).toBe('*');
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm test -- startup.test.ts`
Expected: FAIL — `Cannot find module '../src/app'`.

- [ ] **Step 3: Write `app.ts`**

```typescript
// src/app.ts
import express from 'express';
import { createRouter } from './routes';
import { releasesRepoCors } from './cors';
import type { ServerConfig } from './config';

export function createApp(cfg: ServerConfig): express.Express {
  const app = express();
  app.disable('x-powered-by');
  app.use(express.json({ limit: '1mb' }));
  app.use('/api/releases', releasesRepoCors());
  app.use('/api/releases', createRouter(cfg));
  return app;
}
```

- [ ] **Step 4: Write `index.ts`**

```typescript
// src/index.ts
import { createApp } from './app';
import { loadConfig, ConfigError } from './config';
import { openDb, applySchema } from './db';
import { getSeed } from './catalog';

function main(): void {
  let cfg;
  try {
    cfg = loadConfig();
  } catch (err) {
    if (err instanceof ConfigError) {
      console.error(`[ERROR] ${err.message}`);
      process.exit(1);
    }
    throw err;
  }

  try {
    const db = openDb(cfg);
    applySchema(db);
    db.close();
  } catch (err) {
    console.error(`[ERROR] migrating ${cfg.dbPath} failed: ${(err as Error).message}`);
    process.exit(1);
  }

  const app = createApp(cfg);
  app.listen(cfg.port, () => {
    const db = openDb(cfg, { readonly: true });
    const seed = getSeed(db);
    db.close();
    console.log(`[OK] releases-doorserver listening on ${cfg.port}`);
    console.log(`[INFO] catalog: ${seed.history.length} releases, revision ${seed.revision}`);
  });
}

if (require.main === module) {
  main();
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `npm test -- startup.test.ts`
Expected: PASS, both tests.

- [ ] **Step 6: Run the full test suite**

Run: `npm test`
Expected: PASS, every test file from Tasks 2-8.

- [ ] **Step 7: Typecheck**

Run: `npm run typecheck`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add src/app.ts src/index.ts tests/startup.test.ts
git commit -m "Wire app + entrypoint, boot the server"
```

---

### Task 9: Dockerfile + README

**Files:**
- Create: `/Users/spot/Code/releases-doorserver/Dockerfile`
- Create: `/Users/spot/Code/releases-doorserver/docker-compose.yml`
- Create: `/Users/spot/Code/releases-doorserver/README.md`

**Interfaces:**
- Produces: a runnable container image; no code interface (deployment artifacts only).

- [ ] **Step 1: Check the reference Dockerfile for the base-image/build pattern**

Run: `cat /Users/spot/Code/amiexpress-doorserver/Dockerfile`

Read its Node base image version and multi-stage build shape (build stage running `npm ci && npm run build`, runtime stage copying only `dist/` + `node_modules` production deps) — mirror the same shape below, adjusted for this project having no `web/` frontend to build.

- [ ] **Step 2: Write `Dockerfile`**

```dockerfile
FROM node:22-slim AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
RUN npm run build

FROM node:22-slim AS runtime
WORKDIR /app
ENV NODE_ENV=production
COPY package.json package-lock.json ./
RUN npm ci --omit=dev
COPY --from=build /app/dist ./dist
EXPOSE 3020
CMD ["node", "dist/src/index.js"]
```

- [ ] **Step 3: Write `docker-compose.yml`**

```yaml
services:
  releases-doorserver:
    build: .
    ports:
      - '3020:3020'
    environment:
      RELEASES_DB: /data/releases.db
      RELEASES_WRITE_KEYS: ${RELEASES_WRITE_KEYS}
      PORT: 3020
    volumes:
      - releases-data:/data

volumes:
  releases-data:
```

- [ ] **Step 4: Write `README.md`**

```markdown
# releases-doorserver

Metadata API for Up Rough releases. Forked from `amiexpress-doorserver`'s
config/db/cors conventions, with no file storage (archives live at
`www.uprough.net/releases/<filename>`) and a static shared-key write
endpoint instead of a login-based admin API.

## Configuration

| Variable               | Required | Meaning                                          |
|-------------------------|----------|---------------------------------------------------|
| `RELEASES_DB`            | yes      | path to the SQLite database file                  |
| `RELEASES_WRITE_KEYS`    | yes      | `label:key,label:key` — shared secrets that may POST a new release |
| `PORT`                   | no       | listen port, default 3020                         |

Missing or non-existent paths are a startup failure, never a default.

## Commands

| Task           | Command          |
|-----------------|-------------------|
| dev server      | `npm run dev`     |
| build           | `npm run build`   |
| type-check      | `npm run typecheck` |
| test            | `npm test`         |
| test (CI)       | `npm run test:ci`  |
| start (built)   | `npm start`        |

## First-time setup

The SQLite file must exist before the server starts (the schema is applied
by `index.ts` on boot, but the file itself must already exist for
`RELEASES_DB`'s existence check to pass):

\`\`\`bash
touch /path/to/releases.db
RELEASES_DB=/path/to/releases.db RELEASES_WRITE_KEYS=app:$(openssl rand -hex 32) npm run dev
\`\`\`
```

- [ ] **Step 5: Verify the Dockerfile builds**

Run: `cd /Users/spot/Code/releases-doorserver && docker build -t releases-doorserver:test .`
Expected: build succeeds. (If Docker isn't available in this environment, note that explicitly and skip — this step verifies the Dockerfile syntax and dependency resolution, not a hard requirement for the rest of the plan.)

- [ ] **Step 6: Commit**

```bash
git add Dockerfile docker-compose.yml README.md
git commit -m "Add Docker deployment config and README"
```

---

## Part B — `makerelease` app integration

### Task 10: Simplify shared types for the API-backed model

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/shared/types.ts`

**Interfaces:**
- Produces the new canonical shapes every later task in Part B consumes:
  - `ReleaseHistoryEntry { releaseNumber: number; filenames: string; title: string; category: string; creators: string }`
  - `SeedState { members: MemberEntry[]; nextReleaseNumber: number; history: ReleaseHistoryEntry[]; currentDizText: string; revision: number }`
  - `MakereleaseApi` unchanged in shape (still `loadSeedState`/`pickFilesAndDirs`/`checkCodeAvailability`/`buildRelease`), only the underlying `SeedState` it returns changes.

- [ ] **Step 1: Rewrite the file**

```typescript
// src/shared/types.ts
export interface MemberEntry {
  nickname: string;
  role: string;
}

export interface ReleaseHistoryEntry {
  releaseNumber: number;
  filenames: string;
  title: string;
  category: string;
  creators: string;
}

export type ReleaseFormat = 'LHA' | 'ZIP';

export interface ReleaseFormData {
  code: string;
  format: ReleaseFormat;
  releaseName: string;
  platformTag: string;
  releaseNotesText: string;
  newsText: string;
  dizBannerText: string;
  members: MemberEntry[];
  entryTitle: string;
  entryCategory: string;
  entryCreators: string;
  selectedPaths: string[];
}

export interface GeneratedOutputs {
  nfoText: string;
  dizText: string;
  releaseNumber: number;
}

export interface SeedState {
  members: MemberEntry[];
  nextReleaseNumber: number;
  history: ReleaseHistoryEntry[];
  currentDizText: string;
  revision: number;
}

export interface BuildResult {
  archivePath: string;
  releaseNumber: number;
}

export interface MakereleaseApi {
  loadSeedState(): Promise<SeedState>;
  pickFilesAndDirs(): Promise<string[]>;
  checkCodeAvailability(code: string): Promise<{ exists: boolean }>;
  buildRelease(form: ReleaseFormData): Promise<BuildResult>;
}
```

This removes `ParsedNfo` entirely (nothing local-parses NFOs anymore after
Task 15) and drops `releaseNumberRaw`/`releaseNumberValue`/`rawBlock` from
`ReleaseHistoryEntry` — those existed only to preserve 25 years of
hand-edited formatting quirks byte-for-byte when parsing a local file. The
database is now the single, always-clean source of history, so every entry
renders through one uniform formatter (Task 13) instead of being replayed
verbatim.

- [ ] **Step 2: Typecheck (expect failures — this is intentional, later tasks fix each one)**

Run: `cd /Users/spot/Code/makerelease/app && npm run typecheck`
Expected: FAIL, with errors in `nfo-generator.ts`, `nfo-parser.ts`, `repo-scanner.ts`, `ipc-handlers.ts`, and their test files — every consumer of the types just removed. This is expected and tracked; each subsequent task fixes its own file. Do not attempt to make typecheck pass in this task.

- [ ] **Step 3: Commit**

```bash
git add src/shared/types.ts
git commit -m "Simplify shared types for the API-backed release model

Typecheck is expected to fail until Tasks 11-15 update every consumer."
```

---

### Task 11: Main-process-only config for the write key

**Files:**
- Create: `/Users/spot/Code/makerelease/app/src/main/config.ts`

**Interfaces:**
- Produces: `interface ReleasesClientConfig { baseUrl: string; writeKey: string }`, `loadReleasesClientConfig(env?: NodeJS.ProcessEnv): ReleasesClientConfig`.
- **Security boundary:** this file lives under `src/main/`, which `tsconfig.renderer.json` does not include (verify: `grep include app/tsconfig.renderer.json` shows only `src/renderer/**` and `src/shared/**`). The write key must never move to `src/shared/` — that directory is bundled into the renderer, which is inspectable via devtools even with no UI exposing it.

- [ ] **Step 1: Write the implementation** (no test needed — this is a thin env-var reader with an intentional placeholder default; Task 14's IPC tests cover the config being wired through correctly)

```typescript
// src/main/config.ts
const DEFAULT_BASE_URL = 'https://releases.uprough.net';
const PLACEHOLDER_KEY = 'REPLACE_ME_BEFORE_DISTRIBUTING';

export interface ReleasesClientConfig {
  baseUrl: string;
  writeKey: string;
}

export function loadReleasesClientConfig(env: NodeJS.ProcessEnv = process.env): ReleasesClientConfig {
  const writeKey = env.RELEASES_WRITE_KEY ?? PLACEHOLDER_KEY;
  if (writeKey === PLACEHOLDER_KEY) {
    console.warn(
      '[WARN] RELEASES_WRITE_KEY is not set — using a placeholder that the server will reject. ' +
        'Set RELEASES_WRITE_KEY before distributing this build.',
    );
  }
  return {
    baseUrl: env.RELEASES_API_BASE_URL ?? DEFAULT_BASE_URL,
    writeKey,
  };
}
```

- [ ] **Step 2: Typecheck**

Run: `cd /Users/spot/Code/makerelease/app && npx tsc -p tsconfig.main.json --noEmit`
Expected: PASS (this new file has no dependency on the types changed in Task 10).

- [ ] **Step 3: Commit**

```bash
git add src/main/config.ts
git commit -m "Add main-process-only config for the releases API write key"
```

---

### Task 12: HTTP client for releases-doorserver

**Files:**
- Create: `/Users/spot/Code/makerelease/app/src/main/releases-client.ts`
- Test: `/Users/spot/Code/makerelease/app/test/unit/releases-client.spec.ts`

**Interfaces:**
- Consumes: `ReleaseFormData`, `ReleaseHistoryEntry`, `MemberEntry` (Task 10); `ReleasesClientConfig` (Task 11).
- Produces:
  - `interface SeedResponse { members: MemberEntry[]; nextReleaseNumber: number; history: ReleaseHistoryEntry[]; currentDizText: string | null; revision: number }`
  - `interface PublishResponse { releaseNumber: number; revision: number; history: ReleaseHistoryEntry[] }`
  - `fetchSeed(cfg: ReleasesClientConfig): Promise<SeedResponse>`
  - `checkCodeExists(cfg: ReleasesClientConfig, code: string): Promise<boolean>`
  - `publishRelease(cfg: ReleasesClientConfig, form: ReleaseFormData): Promise<PublishResponse>`
  - `class ReleasesApiError extends Error` — thrown on a non-2xx response or network failure, with a message suitable for surfacing directly in the app's existing error-status UI pattern.

Uses Node's built-in global `fetch` (available in Electron 33's bundled
Node/Chromium, no extra dependency).

- [ ] **Step 1: Write the failing tests**

Mock `global.fetch` directly — no real network call in the test suite, matching the project's existing testing conventions (`nfo-parser`/`nfo-generator` test against fixtures, not live I/O).

```typescript
// test/unit/releases-client.spec.ts
import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import { fetchSeed, checkCodeExists, publishRelease, ReleasesApiError } from '../../src/main/releases-client';
import { ReleaseFormData } from '../../src/shared/types';

const cfg = { baseUrl: 'https://releases.example.test', writeKey: 'secret' };

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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /Users/spot/Code/makerelease/app && npx vitest run test/unit/releases-client.spec.ts`
Expected: FAIL — `Cannot find module '../../src/main/releases-client'`.

- [ ] **Step 3: Write the implementation**

```typescript
// src/main/releases-client.ts
import { MemberEntry, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
import { ReleasesClientConfig } from './config';

export class ReleasesApiError extends Error {}

export interface SeedResponse {
  members: MemberEntry[];
  nextReleaseNumber: number;
  history: ReleaseHistoryEntry[];
  currentDizText: string | null;
  revision: number;
}

export interface PublishResponse {
  releaseNumber: number;
  revision: number;
  history: ReleaseHistoryEntry[];
}

async function getJson<T>(cfg: ReleasesClientConfig, path: string): Promise<T> {
  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}${path}`, { method: 'GET' });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError(
      `releases.uprough.net returned ${response.status}: ${(body as { error?: string }).error ?? 'unknown error'}`,
    );
  }
  return body as T;
}

export async function fetchSeed(cfg: ReleasesClientConfig): Promise<SeedResponse> {
  return getJson<SeedResponse>(cfg, '/api/releases/seed');
}

export async function checkCodeExists(cfg: ReleasesClientConfig, code: string): Promise<boolean> {
  const result = await getJson<{ exists: boolean }>(cfg, `/api/releases/${encodeURIComponent(code)}/exists`);
  return result.exists;
}

function toPublishBody(form: ReleaseFormData): Record<string, unknown> {
  return {
    code: form.code,
    format: form.format,
    releaseName: form.releaseName,
    platformTag: form.platformTag,
    releaseNotes: form.releaseNotesText,
    news: form.newsText,
    dizText: form.dizBannerText,
    entryTitle: form.entryTitle,
    entryCategory: form.entryCategory,
    entryCreators: form.entryCreators,
    members: form.members,
  };
}

export async function publishRelease(cfg: ReleasesClientConfig, form: ReleaseFormData): Promise<PublishResponse> {
  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}/api/releases`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Release-Key': cfg.writeKey },
      body: JSON.stringify(toPublishBody(form)),
    });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError((body as { error?: string }).error ?? `releases.uprough.net returned ${response.status}`);
  }
  return body as PublishResponse;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run test/unit/releases-client.spec.ts`
Expected: PASS, all 6 tests.

- [ ] **Step 5: Commit**

```bash
git add src/main/releases-client.ts test/unit/releases-client.spec.ts
git commit -m "Add HTTP client for releases-doorserver"
```

---

### Task 13: Simplify nfo-generator.ts to render history uniformly

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/main/nfo-generator.ts`
- Modify: `/Users/spot/Code/makerelease/app/test/unit/nfo-generator.spec.ts`

**Interfaces:**
- Consumes: `ReleaseHistoryEntry`, `ReleaseFormData`, `MemberEntry`, `GeneratedOutputs` (Task 10).
- Produces:
  - `renderHistoryEntry(entry: ReleaseHistoryEntry): string[]` (renamed/generalized from the old `renderNewHistoryEntry` — same wrapping/padding logic, now takes one entry object instead of five loose parameters)
  - `renderHistoryBlock(history: ReleaseHistoryEntry[]): string`
  - `renderNfo(history: ReleaseHistoryEntry[], form: ReleaseFormData, releaseNumber: number): string`
  - `generateOutputs(history: ReleaseHistoryEntry[], releaseNumber: number, form: ReleaseFormData): GeneratedOutputs`
  - `renderMemberBlock` and `renderDiz` are unchanged.

- [ ] **Step 1: Update the test file first (TDD — this defines the new contract before the implementation changes)**

The old golden-fixture tests (`renderMemberBlock` against the real `UP-FB006.NFO`, and the `rawBlock`-passthrough history test) depended on `parseNfo`, which is being deleted in Task 15. Replace them with synthetic-but-realistic data built directly as TypeScript literals — this is still a real, meaningful test of the padding/wrapping logic, just no longer sourced by parsing a file.

```typescript
// test/unit/nfo-generator.spec.ts
import { describe, expect, it } from 'vitest';
import { renderMemberBlock, renderHistoryBlock, renderHistoryEntry, renderNfo, generateOutputs } from '../../src/main/nfo-generator';
import { MemberEntry, ReleaseHistoryEntry, ReleaseFormData } from '../../src/shared/types';

const MEMBERS: MemberEntry[] = [
  { nickname: 'the soul collector', role: 'ink' },
  { nickname: 'spot', role: 'leader, ink, stereofonik development' },
  { nickname: 'qwan', role: 'co-leader, stereofonik development' },
];

describe('renderMemberBlock', () => {
  it('pads every nickname to the widest nickname + 2, minimum 20', () => {
    const block = renderMemberBlock(MEMBERS).split('\n');
    // "the soul collector" is 18 chars, so pad width is max(20, 18+2) = 20
    expect(block[0]).toBe('        the soul collector  ink');
    expect(block[1]).toBe('        spot                leader, ink, stereofonik development');
  });

  it('uses the minimum width of 20 when every nickname is short', () => {
    const block = renderMemberBlock([{ nickname: 'ab', role: 'x' }]).split('\n');
    expect(block[0]).toBe('        ab                  x');
  });
});

describe('renderHistoryEntry', () => {
  it('renders the four-line label-padded block for a short entry', () => {
    const entry: ReleaseHistoryEntry = {
      releaseNumber: 208,
      filenames: 'up-fb006.lha',
      title: 'fleisch bitte ad 006',
      category: 'intro (fjälldata 2023)',
      creators: 'nori k, mortimer twang, spot, qwan, exocet',
    };
    const lines = renderHistoryEntry(entry);
    expect(lines[0]).toBe('        release 208 up-fb006.lha');
    expect(lines[1]).toBe('        title       fleisch bitte ad 006');
    expect(lines[2]).toBe('        category    intro (fjälldata 2023)');
    expect(lines[3]).toBe('        creator(s)  nori k, mortimer twang, spot, qwan, exocet');
  });

  it('wraps a long creators line with 20-space continuation indent', () => {
    const entry: ReleaseHistoryEntry = {
      releaseNumber: 204,
      filenames: 'ds!up-a1.dms',
      title: 'asciiexpress issue #001',
      category: 'tool',
      creators: 'k-p/hippopotamus dezign, dino, goto80, phantasm, spot, mortimer twang, sk!n',
    };
    const lines = renderHistoryEntry(entry);
    const creatorLines = lines.slice(3);
    expect(creatorLines.length).toBeGreaterThan(1);
    expect(creatorLines[0].startsWith('        creator(s)  ')).toBe(true);
    for (let i = 1; i < creatorLines.length; i++) {
      expect(creatorLines[i].startsWith(' '.repeat(20))).toBe(true);
      expect(creatorLines[i].startsWith(' '.repeat(21))).toBe(false);
    }
  });
});

describe('renderHistoryBlock', () => {
  it('renders every entry uniformly and joins with a blank line, newest first', () => {
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: 209, filenames: 'up-test0.lha', title: 'test', category: 'Demo', creators: 'spot' },
      { releaseNumber: 208, filenames: 'up-fb006.lha', title: 'fleisch bitte ad 006', category: 'intro', creators: 'nori k' },
    ];
    const block = renderHistoryBlock(history);
    expect(block.startsWith('        release 209 up-test0.lha')).toBe(true);
    expect(block).toContain('        release 208 up-fb006.lha');
    expect(block).toContain('\n\n        release 208');
  });
});

const sampleForm: ReleaseFormData = {
  code: 'TEST0',
  format: 'LHA',
  releaseName: "'TEST RELEASE'",
  platformTag: 'A1200',
  releaseNotesText: 'Some notes.',
  newsText: 'Some news.',
  dizBannerText: 'diz art',
  members: MEMBERS,
  entryTitle: 'test release',
  entryCategory: 'Demo',
  entryCreators: 'spot',
  selectedPaths: [],
};

describe('renderNfo', () => {
  it('embeds the release-notes line, banner, members, and history block', () => {
    const history: ReleaseHistoryEntry[] = [
      { releaseNumber: 217, filenames: 'up-test0.lha', title: 'test release', category: 'Demo', creators: 'spot' },
    ];
    const nfo = renderNfo(history, sampleForm, 217);
    expect(nfo).toContain("Release named 'TEST RELEASE' (A1200) (UP-TEST0.LHA)");
    expect(nfo).toContain('Some notes.');
    expect(nfo).toContain('Some news.');
    expect(nfo).toContain('spot                leader, ink, stereofonik development');
    expect(nfo).toContain('release 217 up-test0.lha');
  });
});

describe('generateOutputs', () => {
  it('uses the passed-in releaseNumber directly, does not recompute it', () => {
    const history: ReleaseHistoryEntry[] = [];
    const result = generateOutputs(history, 42, sampleForm);
    expect(result.releaseNumber).toBe(42);
    expect(result.nfoText).toContain('release 42');
  });
});
```

- [ ] **Step 2: Run the updated test file to verify it fails against the OLD implementation**

Run: `cd /Users/spot/Code/makerelease/app && npx vitest run test/unit/nfo-generator.spec.ts`
Expected: FAIL — the old exports (`renderNewHistoryEntry` with 5 params, old `renderHistoryBlock(newEntryLines, existing)` signature, old `renderNfo(parsed, form, releaseNumber)`, old `generateOutputs(parsed, form)`) don't match the new calls. Confirms the test is exercising the new contract, not accidentally passing against old code.

- [ ] **Step 3: Rewrite the implementation**

```typescript
// src/main/nfo-generator.ts
import { GeneratedOutputs, MemberEntry, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
import {
  BANNER,
  FOOTER_LINE,
  HISTORY_BODY_INDENT,
  HISTORY_CONTINUATION_INDENT,
  HISTORY_WRAP_WIDTH,
  HISTORY_LABEL_FIELD_WIDTH,
  MEMBERS_HEADING,
  MEMBER_LABEL_MIN_WIDTH,
  NEWS_HEADING,
  ONLINE_LINE,
  REL_NOTES_HEADING,
  HISTORY_HEADING,
} from '../shared/constants';
import { toAsciiSafe } from './encoding';

function indentBlock(text: string, indent = HISTORY_BODY_INDENT): string {
  return text
    .split('\n')
    .map((line) => (line.length ? ' '.repeat(indent) + line : line))
    .join('\n');
}

export function renderMemberBlock(members: MemberEntry[]): string {
  const padWidth = Math.max(MEMBER_LABEL_MIN_WIDTH, ...members.map((m) => m.nickname.length + 2));
  return members
    .map((m) => ' '.repeat(HISTORY_BODY_INDENT) + m.nickname.padEnd(padWidth) + m.role)
    .join('\n');
}

function wrapWords(text: string, firstWidth: number, contWidth: number): string[] {
  const words = text.split(/\s+/).filter(Boolean);
  const lines: string[] = [];
  let current = '';
  let width = firstWidth;
  for (const word of words) {
    const candidate = current ? `${current} ${word}` : word;
    if (candidate.length > width && current) {
      lines.push(current);
      current = word;
      width = contWidth;
    } else {
      current = candidate;
    }
  }
  if (current) lines.push(current);
  return lines.length ? lines : [''];
}

export function renderHistoryEntry(entry: ReleaseHistoryEntry): string[] {
  const indent = ' '.repeat(HISTORY_BODY_INDENT);
  const contentWidth = HISTORY_WRAP_WIDTH - HISTORY_CONTINUATION_INDENT;
  const lines: string[] = [`${indent}release ${entry.releaseNumber} ${entry.filenames}`];

  const fields: [string, string][] = [
    ['title', entry.title],
    ['category', entry.category],
    ['creator(s)', entry.creators],
  ];
  for (const [label, value] of fields) {
    const wrapped = wrapWords(value, contentWidth, contentWidth);
    lines.push(`${indent}${label.padEnd(HISTORY_LABEL_FIELD_WIDTH)}${wrapped[0]}`);
    for (let i = 1; i < wrapped.length; i++) {
      lines.push(' '.repeat(HISTORY_CONTINUATION_INDENT) + wrapped[i]);
    }
  }
  return lines;
}

export function renderHistoryBlock(history: ReleaseHistoryEntry[]): string {
  return history.map((entry) => renderHistoryEntry(entry).join('\n')).join('\n\n');
}

export function renderNfo(history: ReleaseHistoryEntry[], form: ReleaseFormData, releaseNumber: number): string {
  const releaseNotesLine = `Release named ${form.releaseName}${
    form.platformTag ? ` (${form.platformTag})` : ''
  } (UP-${form.code.toUpperCase()}.${form.format})`;

  const historyBlock = renderHistoryBlock(history);
  const membersBlock = renderMemberBlock(form.members);

  return [
    BANNER,
    '',
    REL_NOTES_HEADING,
    '',
    ' '.repeat(HISTORY_BODY_INDENT) + releaseNotesLine,
    '',
    indentBlock(form.releaseNotesText.trim()),
    '',
    NEWS_HEADING,
    '',
    indentBlock(form.newsText.trim()),
    '',
    MEMBERS_HEADING,
    '',
    membersBlock,
    '',
    '',
    ONLINE_LINE,
    '',
    '',
    HISTORY_HEADING,
    '',
    historyBlock,
    '',
    '',
    FOOTER_LINE,
    '',
  ].join('\n');
}

export function renderDiz(form: ReleaseFormData): string {
  return toAsciiSafe(form.dizBannerText.trim()) + '\n';
}

export function generateOutputs(
  history: ReleaseHistoryEntry[],
  releaseNumber: number,
  form: ReleaseFormData,
): GeneratedOutputs {
  return {
    nfoText: renderNfo(history, form, releaseNumber),
    dizText: renderDiz(form),
    releaseNumber,
  };
}
```

Note `releaseNumber` in `renderNewHistoryEntry`'s old logic is gone — the
passed-in `releaseNumber` param is unused inside `renderNfo` now (it was
already only used to build `historyBlock`, and `history[0]` already carries
the just-inserted release's number, matching what `POST /releases` returns
per the spec). Keep the parameter for now since `generateOutputs`'s callers
(Task 14) pass it explicitly and it documents intent clearly at the call
site — this is a deliberate small redundancy for readability, not dead code
removed carelessly.

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run test/unit/nfo-generator.spec.ts`
Expected: PASS, all tests.

- [ ] **Step 5: Commit**

```bash
git add src/main/nfo-generator.ts test/unit/nfo-generator.spec.ts
git commit -m "Simplify nfo-generator to render all history entries uniformly

The DB is now the single source of history, so there's no more legacy
hand-edited formatting to preserve verbatim via rawBlock passthrough."
```

---

### Task 14: Rewire ipc-handlers.ts and index.ts to the API

**Files:**
- Modify: `/Users/spot/Code/makerelease/app/src/main/ipc-handlers.ts`
- Modify: `/Users/spot/Code/makerelease/app/src/main/index.ts`

**Interfaces:**
- Consumes: `fetchSeed`/`checkCodeExists`/`publishRelease`/`ReleasesApiError` (Task 12), `loadReleasesClientConfig` (Task 11), `generateOutputs` (Task 13).
- Produces: `registerIpcHandlers(makereleaseRoot: string, releasesConfig: ReleasesClientConfig): void` — same four IPC channel names as before, so `preload.ts` and the renderer need **no changes** (verify this after the fact in Step 4).

- [ ] **Step 1: Rewrite `ipc-handlers.ts`**

```typescript
// src/main/ipc-handlers.ts
import { dialog, ipcMain } from 'electron';
import * as path from 'path';
import { BuildResult, ReleaseFormData, SeedState } from '../shared/types';
import { fetchSeed, checkCodeExists, publishRelease } from './releases-client';
import { generateOutputs } from './nfo-generator';
import { stageRelease } from './staging';
import { packArchive, finalizeRelease } from './archiver';
import { ReleasesClientConfig } from './config';

export function registerIpcHandlers(makereleaseRoot: string, releasesConfig: ReleasesClientConfig): void {
  ipcMain.handle('loadSeedState', async (): Promise<SeedState> => {
    const seed = await fetchSeed(releasesConfig);
    return {
      members: seed.members,
      nextReleaseNumber: seed.nextReleaseNumber,
      history: seed.history,
      currentDizText: seed.currentDizText ?? '',
      revision: seed.revision,
    };
  });

  ipcMain.handle('pickFilesAndDirs', async (): Promise<string[]> => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile', 'openDirectory', 'multiSelections'],
    });
    if (result.canceled) return [];
    return result.filePaths;
  });

  ipcMain.handle('checkCodeAvailability', async (_event, code: string): Promise<{ exists: boolean }> => {
    return { exists: await checkCodeExists(releasesConfig, code) };
  });

  ipcMain.handle('buildRelease', async (_event, form: ReleaseFormData): Promise<BuildResult> => {
    const published = await publishRelease(releasesConfig, form);
    const generated = generateOutputs(published.history, published.releaseNumber, form);
    const outerDir = await stageRelease(form, generated);
    await packArchive(outerDir, form.code, form.format, makereleaseRoot);
    await finalizeRelease(outerDir, form.code, makereleaseRoot);
    return {
      archivePath: path.join(makereleaseRoot, `UP-${form.code.toUpperCase()}.${form.format}`),
      releaseNumber: published.releaseNumber,
    };
  });
}
```

- [ ] **Step 2: Update `index.ts` to load and pass the releases-client config**

```typescript
// src/main/index.ts
import { app, BrowserWindow } from 'electron';
import * as path from 'path';
import { registerIpcHandlers } from './ipc-handlers';
import { loadReleasesClientConfig } from './config';

// __dirname is dist/main/main (tsc preserves the src/main/ subfolder under outDir dist/main).
const MAKERELEASE_ROOT = path.resolve(__dirname, '..', '..', '..', '..');

function createWindow(): void {
  const win = new BrowserWindow({
    width: 900,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, '..', '..', 'preload', 'preload', 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  win.loadFile(path.join(__dirname, '..', '..', 'renderer', 'renderer', 'index.html'));
  win.webContents.on('console-message', (_event, _level, message, line, sourceId) => {
    console.log(`[renderer] ${sourceId}:${line} ${message}`);
  });
  win.webContents.on('did-fail-load', (_event, errorCode, errorDescription, validatedURL) => {
    console.log(`[did-fail-load] ${errorCode} ${errorDescription} ${validatedURL}`);
  });
  win.webContents.on('preload-error', (_event, preloadPath, error) => {
    console.log(`[preload-error] ${preloadPath} ${error.message}\n${error.stack}`);
  });
}

app.whenReady().then(() => {
  const releasesConfig = loadReleasesClientConfig();
  registerIpcHandlers(MAKERELEASE_ROOT, releasesConfig);
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
```

(Only the `loadReleasesClientConfig` import, the `releasesConfig` local, and
the `registerIpcHandlers` call signature change — every other line, including
the existing console-message/did-fail-load/preload-error diagnostics added
earlier this session, is unchanged.)

- [ ] **Step 3: Typecheck main**

Run: `cd /Users/spot/Code/makerelease/app && npx tsc -p tsconfig.main.json --noEmit`
Expected: PASS.

- [ ] **Step 4: Confirm preload.ts and the renderer need no changes**

Run: `grep -n "loadSeedState\|checkCodeAvailability\|buildRelease\|pickFilesAndDirs" src/preload/preload.ts src/renderer/main.ts`
Expected: the same four IPC method names appear, unchanged — `MakereleaseApi`'s shape (Task 10) didn't change, only `SeedState`'s internal fields did, and the renderer already only reads `seed.members`/`seed.nextReleaseNumber`/`seed.currentDizText` (verify with `grep -n "seed\." src/renderer/main.ts` — it does not reference `seed.history` or the old `seed.sourceNfoPath` anywhere, so no renderer edit is needed).

- [ ] **Step 5: Commit**

```bash
git add src/main/ipc-handlers.ts src/main/index.ts
git commit -m "Rewire IPC handlers to call releases-doorserver instead of scanning locally"
```

---

### Task 15: Delete dead local-parsing code

**Files:**
- Delete: `/Users/spot/Code/makerelease/app/src/main/nfo-parser.ts`
- Delete: `/Users/spot/Code/makerelease/app/src/main/repo-scanner.ts`
- Delete: `/Users/spot/Code/makerelease/app/test/unit/nfo-parser.spec.ts`
- Delete: `/Users/spot/Code/makerelease/app/test/unit/repo-scanner.spec.ts`
- Delete: `/Users/spot/Code/makerelease/app/test/fixtures/UP-FB006.NFO`
- Modify: `/Users/spot/Code/makerelease/app/src/main/encoding.ts` (remove now-unused `readLatin1`)

**Interfaces:**
- Consumes: nothing new.
- Produces: nothing new — this task only removes code no longer reachable after Tasks 12-14, per the project's "if certain something is unused, delete completely" convention.

- [ ] **Step 1: Confirm nothing still imports the files being deleted**

Run: `cd /Users/spot/Code/makerelease/app && grep -rn "nfo-parser\|repo-scanner" src/ test/`
Expected: no matches (Task 14's rewrite of `ipc-handlers.ts` already dropped the `repo-scanner` import; nothing else ever imported `nfo-parser` directly except `repo-scanner.ts` itself). If this shows a match, stop and resolve it before deleting — do not delete a file something still imports.

- [ ] **Step 2: Confirm `readLatin1` is unused**

Run: `grep -rn "readLatin1" src/ test/`
Expected: only its own definition in `encoding.ts` — no callers anywhere else (it was previously called from `repo-scanner.ts`, being deleted in this same task, and from the old `ipc-handlers.ts` diz-read code, already replaced in Task 14).

- [ ] **Step 3: Delete the files**

```bash
cd /Users/spot/Code/makerelease/app
rm src/main/nfo-parser.ts
rm src/main/repo-scanner.ts
rm test/unit/nfo-parser.spec.ts
rm test/unit/repo-scanner.spec.ts
rm test/fixtures/UP-FB006.NFO
rmdir test/fixtures 2>/dev/null || true
```

- [ ] **Step 4: Remove `readLatin1` from `encoding.ts`**

```typescript
// src/main/encoding.ts
import * as fs from 'fs/promises';

const ASCII_TRANSLITERATIONS: Record<string, string> = {
  ä: 'ae', ö: 'oe', å: 'aa', ü: 'ue', é: 'e', è: 'e', ê: 'e',
  Ä: 'Ae', Ö: 'Oe', Å: 'Aa', Ü: 'Ue', É: 'E',
  ñ: 'n', ç: 'c', ø: 'oe', Ø: 'Oe', æ: 'ae', Æ: 'Ae',
};

export function toAsciiSafe(input: string): string {
  let out = '';
  for (const char of input) {
    if (char.charCodeAt(0) <= 0x7f) {
      out += char;
    } else {
      out += ASCII_TRANSLITERATIONS[char] ?? '?';
    }
  }
  return out;
}

export async function writeLatin1(filePath: string, text: string): Promise<void> {
  await fs.writeFile(filePath, Buffer.from(text, 'latin1'));
}
```

- [ ] **Step 5: Full typecheck across all three tsconfigs**

Run: `npm run typecheck`
Expected: PASS — this is the point where every error introduced by Task 10's type simplification is now resolved.

- [ ] **Step 6: Full test suite**

Run: `npm test`
Expected: PASS — remaining suites are `encoding.spec.ts`, `nfo-generator.spec.ts` (Task 13), `diz-grid.spec.ts`, `releases-client.spec.ts` (Task 12). `nfo-parser.spec.ts` and `repo-scanner.spec.ts` no longer exist to run.

- [ ] **Step 7: Full renderer + main + preload build**

Run: `npm run build`
Expected: succeeds cleanly.

- [ ] **Step 8: Commit**

```bash
git add -u src/main/nfo-parser.ts src/main/repo-scanner.ts test/unit/nfo-parser.spec.ts test/unit/repo-scanner.spec.ts test/fixtures/UP-FB006.NFO src/main/encoding.ts
git commit -m "Delete local NFO-parsing code, now dead after the API migration"
```

---

### Task 16: End-to-end verification

**Files:** none (verification only).

- [ ] **Step 1: Start releases-doorserver locally**

```bash
cd /Users/spot/Code/releases-doorserver
touch /tmp/releases-e2e.db
RELEASES_DB=/tmp/releases-e2e.db RELEASES_WRITE_KEYS=app:e2e-test-key npm run dev
```

Expected: logs `[OK] releases-doorserver listening on 3020` and `[INFO] catalog: 0 releases, revision 0`.

- [ ] **Step 2: Confirm the empty-catalog seed response**

Run (new terminal): `curl -s http://localhost:3020/api/releases/seed | python3 -m json.tool`
Expected: `{"members": [], "nextReleaseNumber": 1, "history": [], "currentDizText": null, "revision": 0}`.

- [ ] **Step 3: Publish a release directly via curl (sanity check before involving the app)**

```bash
curl -s -X POST http://localhost:3020/api/releases \
  -H 'Content-Type: application/json' \
  -H 'X-Release-Key: e2e-test-key' \
  -d '{"code":"TEST0","format":"LHA","releaseName":"'"'"'TEST'"'"'","platformTag":"A1200","releaseNotes":"notes","news":"news","dizText":"diz","entryTitle":"test","entryCategory":"Demo","entryCreators":"spot","members":[{"nickname":"spot","role":"leader"}]}' \
  | python3 -m json.tool
```

Expected: `{"releaseNumber": 1, "revision": 1, "history": [{"releaseNumber": 1, "filenames": "up-test0.lha", ...}]}`.

- [ ] **Step 4: Point the app at the local server and run it**

```bash
cd /Users/spot/Code/makerelease/app
npm run build
RELEASES_API_BASE_URL=http://localhost:3020 RELEASES_WRITE_KEY=e2e-test-key ./node_modules/.bin/electron .
```

Expected: the app window opens, the member table populates with `spot` / `leader` (seeded from Step 3's publish), and the release-number preview shows `release 2 up-<code>.lha`.

- [ ] **Step 5: Build a real throwaway release through the app UI**

Fill the form with a throwaway 5-char code (e.g. `TEST1`), select a small dummy file, submit. Confirm the app reports success with `releaseNumber: 2` and a local archive path under `/Users/spot/Code/makerelease/UP-TEST1.LHA`.

- [ ] **Step 6: Verify the built archive's NFO reflects the server-driven history**

```bash
lha x /Users/spot/Code/makerelease/UP-TEST1.LHA -w:/tmp/test1-extract
cat /tmp/test1-extract/UP-TEST1/UP-TEST1.NFO
```

Expected: the "released so far" section shows `release 2 up-test1.lha` first, then `release 1 up-test0.lha` (from Step 3) — both rendered through the same uniform formatter (Task 13), confirming the end-to-end flow: server-assigned number -> app-generated NFO -> packed archive.

- [ ] **Step 7: Verify the concurrent-build fix directly**

With the local server still running, run two `POST /api/releases` requests at once for different codes and confirm they get different, sequential numbers (this already has unit coverage in Task 4's `catalog.test.ts`, but confirming it once against the real running server closes the loop):

```bash
curl -s -X POST http://localhost:3020/api/releases -H 'Content-Type: application/json' -H 'X-Release-Key: e2e-test-key' \
  -d '{"code":"CONC1","format":"LHA","releaseName":"a","platformTag":"","releaseNotes":"n","news":"n","dizText":"d","entryTitle":"t","entryCategory":"Demo","entryCreators":"c","members":[]}' &
curl -s -X POST http://localhost:3020/api/releases -H 'Content-Type: application/json' -H 'X-Release-Key: e2e-test-key' \
  -d '{"code":"CONC2","format":"LHA","releaseName":"a","platformTag":"","releaseNotes":"n","news":"n","dizText":"d","entryTitle":"t","entryCategory":"Demo","entryCreators":"c","members":[]}' &
wait
```

Expected: the two responses report `releaseNumber: 3` and `releaseNumber: 4` (in either order), never the same number twice.

- [ ] **Step 8: Clean up**

```bash
rm -f /Users/spot/Code/makerelease/UP-TEST1.LHA
rm -rf /Users/spot/Code/makerelease/UP-TEST1
rm -f /tmp/releases-e2e.db
rm -rf /tmp/test1-extract
```

Stop both the local `releases-doorserver` process and the Electron app.

- [ ] **Step 9: Report results**

Confirm and note: all 16 tasks complete, full test suites green on both repos, typecheck clean, and the manual end-to-end walkthrough above passed every step. Any deviation gets reported as "N of 16 done, task X open" — not "done" — per this project's reachability standard.
