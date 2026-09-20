import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { submitDoor } from '../../src/main/door-repo-client';

let archivePath: string;

beforeEach(() => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'door-submit-'));
  archivePath = path.join(dir, 'UP-MD15.LHA');
  fs.writeFileSync(archivePath, 'archive bytes');
  global.fetch = vi.fn() as any;
});

afterEach(() => {
  vi.restoreAllMocks();
});

describe('submitDoor', () => {
  it('posts the archive and passes the queue message back', async () => {
    (global.fetch as any).mockResolvedValue({
      ok: true,
      status: 202,
      json: async () => ({ ok: true, id: 12, message: 'Thank you - it is in the queue and a curator will look at it.' }),
    });

    const result = await submitDoor(archivePath);

    expect(result).toEqual({
      ok: true,
      id: 12,
      message: 'Thank you - it is in the queue and a curator will look at it.',
    });
    const [url, init] = (global.fetch as any).mock.calls[0];
    expect(url).toBe('https://doors.uprough.net/api/door-repo/submissions');
    expect(init.method).toBe('POST');
    expect(init.body).toBeInstanceOf(FormData);
  });

  it('reports a refusal in the door server own words', async () => {
    // A rebuild of the same release is refused on its sha256, and that
    // sentence is more useful than anything this end could invent.
    (global.fetch as any).mockResolvedValue({
      ok: false,
      status: 409,
      json: async () => ({ error: 'the repository already has that archive, as up-md15.lha' }),
    });

    const result = await submitDoor(archivePath);

    expect(result.ok).toBe(false);
    expect(result.message).toBe('the repository already has that archive, as up-md15.lha');
  });

  it('reports an unreachable door server rather than throwing', async () => {
    (global.fetch as any).mockRejectedValue(new Error('ECONNREFUSED'));

    const result = await submitDoor(archivePath);

    expect(result.ok).toBe(false);
    expect(result.message).toMatch(/could not reach the door repository/);
  });

  it('still says something useful when the answer carries no message', async () => {
    (global.fetch as any).mockResolvedValue({ ok: false, status: 500, json: async () => ({}) });
    await expect(submitDoor(archivePath)).resolves.toMatchObject({ ok: false, message: expect.stringContaining('500') });
  });
});
