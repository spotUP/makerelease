import { describe, it, expect, vi, beforeEach } from 'vitest';

type ExecFileCall = { file: string; args: string[]; opts: { cwd?: string; env?: NodeJS.ProcessEnv } };

const calls: ExecFileCall[] = [];
let failCall: { index: number; error: Error } | null = null;
const written: Array<{ filePath: string; bytes: Buffer }> = [];

vi.mock('child_process', () => ({
  execFile: (file: string, args: string[], opts: any, cb: (e: unknown, out: string, err: string) => void) => {
    const index = calls.push({ file, args, opts }) - 1;
    if (failCall && failCall.index === index) cb(failCall.error, '', '');
    else cb(null, '', '');
  },
}));

vi.mock('fs/promises', () => ({
  writeFile: async (filePath: string, bytes: Buffer) => {
    written.push({ filePath, bytes });
  },
}));

const { uploadToAminet, AMINET_UPLOAD_URL } = await import('../../src/main/aminet');

const readmeText = [
  'Short:        A500 HDD demo',
  'Uploader:     spot@uprough.net (Spot)',
  'Author:       Up Rough',
  'Type:         demo/aga',
  'Architecture: m68k-amigaos',
  'Distribution: Aminet',
  '',
  'Hypocrisy, our AGA demo.',
  '',
].join('\n');

const request = { readmeText, uploaderEmail: 'spot@uprough.net' };

function enoent(): NodeJS.ErrnoException {
  const err: NodeJS.ErrnoException = new Error('spawn curl ENOENT');
  err.code = 'ENOENT';
  return err;
}

describe('uploadToAminet', () => {
  beforeEach(() => {
    calls.length = 0;
    written.length = 0;
    failCall = null;
  });

  it('writes the readme next to the archive, then curls the archive and the readme, in that order, as anonymous with the email', async () => {
    const result = await uploadToAminet(request, '/out/UP-HYPOC.LHA');
    expect(result.ok).toBe(true);
    expect(result.readmePath).toBe('/out/up-hypoc.readme');
    expect(written).toHaveLength(1);
    expect(written[0].filePath).toBe('/out/up-hypoc.readme');
    expect(calls.map((c) => c.file)).toEqual(['curl', 'curl']);
    expect(calls[0].args).toEqual([
      '--silent',
      '--show-error',
      '--fail',
      '--ftp-pasv',
      '--user',
      'anonymous:spot@uprough.net',
      '--upload-file',
      '/out/UP-HYPOC.LHA',
      `${AMINET_UPLOAD_URL}up-hypoc.lha`,
    ]);
    expect(calls[1].args.slice(-2)).toEqual(['/out/up-hypoc.readme', `${AMINET_UPLOAD_URL}up-hypoc.readme`]);
    expect(result.message).toContain('up-hypoc.lha and up-hypoc.readme');
  });

  it('normalises the readme to LF, ASCII and one trailing newline before writing it', async () => {
    await uploadToAminet(
      { ...request, readmeText: readmeText.replace(/\n/g, '\r\n').replace('demo.', 'démo.').trimEnd() },
      '/out/UP-HYPOC.LHA',
    );
    const bytes = written[0].bytes.toString('latin1');
    expect(bytes).not.toContain('\r');
    expect(bytes).toMatch(/AGA demo\.\n$/);
    expect(bytes).not.toContain('é');
    expect(bytes.endsWith('\n\n')).toBe(false);
  });

  it('refuses a readme that fails the Aminet checklist, before writing or uploading anything', async () => {
    const result = await uploadToAminet({ ...request, readmeText: `${readmeText}${'z'.repeat(79)}\n` }, '/out/UP-HYPOC.LHA');
    expect(result.ok).toBe(false);
    expect(result.message).toBe('not uploaded to Aminet: line 9 is 79 characters; Aminet allows 78');
    expect(written).toHaveLength(0);
    expect(calls).toHaveLength(0);
  });

  it('refuses an archive name Aminet would not take', async () => {
    const result = await uploadToAminet(request, `/out/${'A'.repeat(30)}.LHA`);
    expect(result.ok).toBe(false);
    expect(result.message).toContain('Aminet allows 30');
    expect(calls).toHaveLength(0);
  });

  it('reports a missing curl with an install hint instead of throwing', async () => {
    failCall = { index: 0, error: enoent() };
    const result = await uploadToAminet(request, '/out/UP-HYPOC.LHA');
    expect(result.ok).toBe(false);
    expect(result.message).toMatch(/^Aminet upload of up-hypoc\.lha failed: "curl" was not found on your PATH/);
    expect(calls).toHaveLength(1);
  });

  it('stops after the archive upload fails, so no orphan readme is left on Aminet', async () => {
    failCall = { index: 0, error: new Error('curl: (67) Access denied: 530') };
    const result = await uploadToAminet(request, '/out/UP-HYPOC.LHA');
    expect(result.ok).toBe(false);
    expect(result.message).toBe('Aminet upload of up-hypoc.lha failed: curl: (67) Access denied: 530');
    expect(result.readmePath).toBe('/out/up-hypoc.readme');
    expect(calls).toHaveLength(1);
  });

  it('names the readme when its upload is the one that fails', async () => {
    failCall = { index: 1, error: new Error('curl: (28) Timeout') };
    const result = await uploadToAminet(request, '/out/UP-HYPOC.LHA');
    expect(result.ok).toBe(false);
    expect(result.message).toBe('Aminet upload of up-hypoc.readme failed: curl: (28) Timeout');
    expect(calls).toHaveLength(2);
  });
});
