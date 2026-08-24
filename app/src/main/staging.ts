import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { GeneratedOutputs, ReleaseFormData } from '../shared/types';
import { writeLatin1 } from './encoding';
import { copyRecursive, baseName } from './fs-helpers';

export async function stageRelease(form: ReleaseFormData, generated: GeneratedOutputs): Promise<string> {
  const tmpRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-'));
  const code = form.code.toUpperCase();
  const outerDir = path.join(tmpRoot, `UP-${code}`);
  const innerDir = path.join(outerDir, `UP-${code}`);
  await fs.mkdir(innerDir, { recursive: true });

  await writeLatin1(path.join(outerDir, 'file_id.diz'), generated.dizText);
  await writeLatin1(path.join(innerDir, `UP-${code}.NFO`), generated.nfoText);

  for (const src of form.selectedPaths) {
    await copyRecursive(src, path.join(innerDir, baseName(src)));
  }

  return outerDir;
}
