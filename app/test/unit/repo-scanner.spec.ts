import { describe, expect, it, afterEach } from 'vitest';
import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { scanForLatestNfo, codeExists } from '../../src/main/repo-scanner';

async function writeFakeRelease(root: string, code: string, releaseNumber: number): Promise<void> {
  const innerDir = path.join(root, `UP-${code}`, `UP-${code}`);
  await fs.mkdir(innerDir, { recursive: true });
  const nfo = `
        banner

        [ r e l e a s e   n o t e s ]

        Release named 'TEST' (UP-${code}.LHA)

        notes

        [ n e w s ]

        news

        a c t i v e   u p   r o u g h   h e r o e s

        spot                leader


        online         www.uprough.net


        r e l e a s e d   s o   f a r

        release ${releaseNumber} up-${code.toLowerCase()}.lha
        title       test
        category    intro
        creator(s)  spot


        c a t c h \`e m   a l l   a t   www.uprough.net
`;
  await fs.writeFile(path.join(innerDir, `UP-${code}.NFO`), nfo, 'latin1');
}

describe('scanForLatestNfo', () => {
  let tmpRoot: string;

  afterEach(async () => {
    if (tmpRoot) await fs.rm(tmpRoot, { recursive: true, force: true });
  });

  it('picks the release with the highest release number, not by mtime', async () => {
    tmpRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'scanner-test-'));
    await writeFakeRelease(tmpRoot, 'AAAAA', 5);
    await writeFakeRelease(tmpRoot, 'BBBBB', 12);

    const result = await scanForLatestNfo(tmpRoot);
    expect(result.maxReleaseNumber).toBe(12);
  });

  it('reports codeExists correctly', async () => {
    tmpRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'scanner-test-'));
    await writeFakeRelease(tmpRoot, 'AAAAA', 5);

    expect(await codeExists(tmpRoot, 'AAAAA')).toBe(true);
    expect(await codeExists(tmpRoot, 'ZZZZZ')).toBe(false);
  });
});
