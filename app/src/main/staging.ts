import * as fs from 'fs/promises';
import * as os from 'os';
import * as path from 'path';
import { GeneratedOutputs, ReleaseFormData } from '../shared/types';
import type { ProfileSummary } from '../shared/profile';
import { releaseBaseName } from '../shared/naming';
import { writeLatin1 } from './encoding';
import { copyRecursive, baseName, isDirectory } from './fs-helpers';

export async function stageRelease(
  profile: Pick<ProfileSummary, 'filePrefix'>,
  form: ReleaseFormData,
  generated: GeneratedOutputs,
): Promise<string> {
  const tmpRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'makerelease-'));
  const releaseDir = releaseBaseName(profile, form.code);
  const outerDir = path.join(tmpRoot, releaseDir);
  const innerDir = path.join(outerDir, releaseDir);
  await fs.mkdir(innerDir, { recursive: true });

  await writeLatin1(path.join(outerDir, 'file_id.diz'), generated.dizText);
  if (generated.nfoText !== null) {
    await writeLatin1(path.join(innerDir, `${releaseDir}.NFO`), generated.nfoText);
  }

  for (const src of form.selectedPaths) {
    // A selected folder that is itself called <PREFIX><CODE> is the release folder
    // the user already assembled by hand, not a subdirectory of the release.
    // Copying it as-is produced <BASE>/<BASE>/<BASE>/<files> in the
    // shipped archive; its contents belong directly in the release dir.
    if (baseName(src).toUpperCase() === releaseDir.toUpperCase() && (await isDirectory(src))) {
      for (const entry of await fs.readdir(src)) {
        await copyRecursive(path.join(src, entry), path.join(innerDir, entry));
      }
      continue;
    }
    await copyRecursive(src, path.join(innerDir, baseName(src)));
  }

  return outerDir;
}
