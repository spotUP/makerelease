import { describe, it, expect, vi, beforeEach } from 'vitest';

type ExecFileCall = { file: string; args: string[]; opts: { cwd?: string; env?: NodeJS.ProcessEnv } };

const calls: ExecFileCall[] = [];
let nextError: NodeJS.ErrnoException | null = null;

vi.mock('child_process', () => ({
  execFile: (file: string, args: string[], opts: any, cb: (e: unknown, out: string, err: string) => void) => {
    calls.push({ file, args, opts });
    if (nextError) cb(nextError, '', '');
    else cb(null, '', '');
  },
}));

const { packArchive } = await import('../../src/main/archiver');

const UR = { filePrefix: 'UP-' };

function enoent(bin: string): NodeJS.ErrnoException {
  const err: NodeJS.ErrnoException = new Error(`spawn ${bin} ENOENT`);
  err.code = 'ENOENT';
  return err;
}

describe('packArchive', () => {
  beforeEach(() => {
    calls.length = 0;
    nextError = null;
  });

  it('invokes lha by name so it resolves from PATH, not a hardcoded macOS path', async () => {
    await packArchive(UR, '/staging/outer', 'evk26', 'LHA', '/out');
    expect(calls[0].file).toBe('lha');
    expect(calls[0].file).not.toContain('/opt/homebrew');
    expect(calls[0].args).toEqual(['a', '/out/UP-EVK26.LHA', 'file_id.diz', 'UP-EVK26']);
    expect(calls[0].opts.cwd).toBe('/staging/outer');
  });

  it('invokes zip by name so it resolves from PATH, not a hardcoded absolute path', async () => {
    await packArchive(UR, '/staging/outer', 'evk26', 'ZIP', '/out');
    expect(calls[0].file).toBe('zip');
    expect(calls[0].file).not.toContain('/usr/bin');
    expect(calls[0].args).toEqual(['-r', '/out/UP-EVK26.ZIP', 'file_id.diz', 'UP-EVK26']);
  });

  it('appends the platform install dirs to PATH, because GUI launches inherit a minimal PATH', async () => {
    await packArchive(UR, '/staging/outer', 'evk26', 'LHA', '/out');
    const inheritedPath = process.env.PATH ?? '';
    const passedPath = calls[0].opts.env?.PATH ?? '';
    expect(passedPath.startsWith(inheritedPath)).toBe(true);
    if (process.platform === 'darwin') expect(passedPath).toContain('/opt/homebrew/bin');
    if (process.platform === 'linux') expect(passedPath).toContain('/usr/local/bin');
  });

  it('reports a missing archiver with an install hint instead of a raw spawn ENOENT', async () => {
    nextError = enoent('lha');
    await expect(packArchive(UR, '/staging/outer', 'evk26', 'LHA', '/out')).rejects.toThrow(
      /"lha" was not found on your PATH/,
    );
  });

  it('names the missing binary that actually failed', async () => {
    nextError = enoent('zip');
    await expect(packArchive(UR, '/staging/outer', 'evk26', 'ZIP', '/out')).rejects.toThrow(
      /"zip" was not found on your PATH/,
    );
  });

  it('still surfaces a real archiver failure unchanged rather than blaming PATH', async () => {
    const failed: NodeJS.ErrnoException = new Error('lha: cannot write archive: No space left on device');
    failed.code = 1 as unknown as string;
    nextError = failed;
    await expect(packArchive(UR, '/staging/outer', 'evk26', 'LHA', '/out')).rejects.toThrow(/No space left on device/);
  });
});
