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
