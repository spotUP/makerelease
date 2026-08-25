import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { GeneratedOutputs, ReleaseFormData } from '../shared/types';
import { writeLatin1 } from './encoding';
import { copyRecursive, baseName, isDirectory } from './fs-helpers';

export async function stageRelease(form: ReleaseFormData, generated: GeneratedOutputs): Promise<string> {
  const tmpRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-'));
  const code = form.code.toUpperCase();
  const outerDir = path.join(tmpRoot, `UP-${code}`);
  const innerDir = path.join(outerDir, `UP-${code}`);
  await fs.mkdir(innerDir, { recursive: true });

  await writeLatin1(path.join(outerDir, 'file_id.diz'), generated.dizText);
  await writeLatin1(path.join(innerDir, `UP-${code}.NFO`), generated.nfoText);

  for (const src of form.selectedPaths) {
    // A selected folder that is itself called UP-<CODE> is the release folder
    // the user already assembled by hand, not a subdirectory of the release.
    // Copying it as-is produced UP-EVK26/UP-EVK26/UP-EVK26/<files> in the
    // shipped archive; its contents belong directly in the release dir.
    if (baseName(src).toUpperCase() === `UP-${code}` && (await isDirectory(src))) {
      for (const entry of await fs.readdir(src)) {
        await copyRecursive(path.join(src, entry), path.join(innerDir, entry));
      }
      continue;
    }
    await copyRecursive(src, path.join(innerDir, baseName(src)));
  }

  return outerDir;
}
