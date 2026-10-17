import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import {
  ensureExampleProfile,
  isProfileLoadError,
  listProfiles,
  loadProfile,
  readTemplates,
} from '../../src/main/profiles';
import { EXAMPLE_DIZ_TEMPLATE, EXAMPLE_NFO_TEMPLATE, EXAMPLE_PROFILE_JSON } from '../../src/main/example-profile';

let dir: string;

async function writeProfile(id: string, body: string): Promise<void> {
  await fs.mkdir(path.join(dir, id), { recursive: true });
  await fs.writeFile(path.join(dir, id, 'profile.json'), body, 'utf8');
}

const VALID = JSON.stringify({ label: 'Demo', url: 'https://demo.net', renderer: 'placeholder', filePrefix: 'DM-' });

beforeEach(async () => {
  dir = path.join(await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-profiles-')), 'profiles');
});

afterEach(async () => {
  await fs.rm(path.dirname(dir), { recursive: true, force: true });
});

describe('profiles on disk', () => {
  it('lists every profile folder sorted by id, skipping files and hidden folders', async () => {
    await writeProfile('zeta', VALID);
    await writeProfile('alpha', VALID);
    await writeProfile('.hidden', VALID);
    await fs.writeFile(path.join(dir, 'notes.txt'), 'x');

    const entries = await listProfiles(dir);

    expect(entries.map((e) => e.id)).toEqual(['alpha', 'zeta']);
    expect(entries.every((e) => !isProfileLoadError(e))).toBe(true);
  });

  it('reports a broken profile in place instead of throwing, so the others still load', async () => {
    await writeProfile('good', VALID);
    await writeProfile('badjson', '{ nope');
    await writeProfile('invalid', JSON.stringify({ label: 'x', url: 'y', renderer: 'fancy' }));
    await fs.mkdir(path.join(dir, 'empty'));

    const byId = Object.fromEntries((await listProfiles(dir)).map((e) => [e.id, e]));

    expect(isProfileLoadError(byId.good)).toBe(false);
    expect((byId.badjson as { error: string }).error).toMatch(/is not valid JSON/);
    expect((byId.invalid as { error: string }).error).toMatch(/"renderer" must be one of/);
    expect((byId.empty as { error: string }).error).toMatch(/could not be read/);
  });

  it('returns no profiles when the folder does not exist', async () => {
    expect(await listProfiles(path.join(dir, 'missing'))).toEqual([]);
  });

  it('loadProfile names the file in its error', async () => {
    await expect(loadProfile(dir, 'nobody')).rejects.toThrow(path.join(dir, 'nobody', 'profile.json'));
  });

  it('writes the example profile on first launch, byte for byte', async () => {
    await ensureExampleProfile(dir);

    expect(await fs.readFile(path.join(dir, 'example', 'profile.json'), 'utf8')).toBe(EXAMPLE_PROFILE_JSON);
    expect(await fs.readFile(path.join(dir, 'example', 'release.nfo'))).toEqual(Buffer.from(EXAMPLE_NFO_TEMPLATE, 'latin1'));
    expect(await fs.readFile(path.join(dir, 'example', 'file_id.diz'))).toEqual(Buffer.from(EXAMPLE_DIZ_TEMPLATE, 'latin1'));
    const example = await loadProfile(dir, 'example');
    expect(example).toMatchObject({ filePrefix: 'EX-', renderer: 'placeholder', backend: 'none' });
  });

  it('never writes the example again once the profiles folder exists, so a deleted example stays deleted', async () => {
    await ensureExampleProfile(dir);
    await fs.rm(path.join(dir, 'example'), { recursive: true });

    await ensureExampleProfile(dir);

    expect(await fs.readdir(dir)).toEqual([]);
  });

  it('reads templates as Latin-1, keeping high characters as single bytes', async () => {
    await writeProfile('demo', VALID);
    await fs.writeFile(path.join(dir, 'demo', 'release.nfo'), Buffer.from([0x52, 0xe9, 0xdb, 0x0a]));
    await fs.writeFile(path.join(dir, 'demo', 'file_id.diz'), Buffer.from([0xb0, 0xb1, 0xb2]));

    const templates = await readTemplates(dir, 'demo');

    expect(templates.nfo).toBe('RéÛ\n');
    expect(templates.diz).toBe('°±²');
  });

  it('turns CRLF and lone CR line endings into LF', async () => {
    await writeProfile('demo', VALID);
    await fs.writeFile(path.join(dir, 'demo', 'release.nfo'), 'a\r\nb\rc\n', 'latin1');
    await fs.writeFile(path.join(dir, 'demo', 'file_id.diz'), 'x\r\n', 'latin1');

    expect(await readTemplates(dir, 'demo')).toEqual({ nfo: 'a\nb\nc\n', diz: 'x\n' });
  });

  it('reads a missing release.nfo as no NFO template', async () => {
    await writeProfile('demo', VALID);
    await fs.writeFile(path.join(dir, 'demo', 'file_id.diz'), 'x\n', 'latin1');

    expect(await readTemplates(dir, 'demo')).toEqual({ nfo: null, diz: 'x\n' });
  });

  it('refuses a profile without file_id.diz, naming the file', async () => {
    await writeProfile('demo', VALID);
    await fs.writeFile(path.join(dir, 'demo', 'release.nfo'), 'x\n', 'latin1');

    await expect(readTemplates(dir, 'demo')).rejects.toThrow('file_id.diz: could not be read');
  });
});
