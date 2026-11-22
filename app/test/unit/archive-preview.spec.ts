import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { describe, expect, it } from 'vitest';
import { previewArchive } from '../../src/main/archive-preview';

async function tree(spec: Record<string, string>): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'archprev-'));
  for (const [rel, text] of Object.entries(spec)) {
    await fs.mkdir(path.dirname(path.join(root, rel)), { recursive: true });
    await fs.writeFile(path.join(root, rel), text);
  }
  return root;
}

describe('previewArchive', () => {
  it('flags a folder that only wraps one more folder and names the folder to flatten to', async () => {
    const root = await tree({ 'wrap/inner/a.txt': 'a', 'wrap/inner/b.txt': 'b' });
    const result = await previewArchive('REL', [path.join(root, 'wrap')]);
    expect(result.nested).toEqual([{ path: path.join(root, 'wrap'), flattenTo: path.join(root, 'wrap', 'inner') }]);
    expect(result.entries).toContain('REL/wrap/inner/a.txt');
  });

  it('does not flag a folder with real content beside its subfolder', async () => {
    const root = await tree({ 'pack/readme.txt': 'r', 'pack/sub/a.txt': 'a' });
    expect((await previewArchive('REL', [path.join(root, 'pack')])).nested).toEqual([]);
  });

  it('merges a folder named like the release instead of nesting it', async () => {
    const root = await tree({ 'rel/a.txt': 'a' });
    const result = await previewArchive('REL', [path.join(root, 'rel')]);
    expect(result.entries).toContain('REL/a.txt');
  });
});
