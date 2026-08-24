import { dialog, ipcMain } from 'electron';
import * as path from 'path';
import { BuildResult, ReleaseFormData, SeedState } from '../shared/types';
import { scanForLatestNfo, codeExists } from './repo-scanner';
import { generateOutputs } from './nfo-generator';
import { stageRelease } from './staging';
import { packArchive, finalizeRelease } from './archiver';

export function registerIpcHandlers(makereleaseRoot: string): void {
  ipcMain.handle('loadSeedState', async (): Promise<SeedState> => {
    const parsed = await scanForLatestNfo(makereleaseRoot);
    return {
      members: parsed.members,
      nextReleaseNumber: parsed.maxReleaseNumber + 1,
      sourceNfoPath: parsed.sourceFilePath,
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
    return { exists: await codeExists(makereleaseRoot, code) };
  });

  ipcMain.handle('buildRelease', async (_event, form: ReleaseFormData): Promise<BuildResult> => {
    const parsed = await scanForLatestNfo(makereleaseRoot);
    const generated = generateOutputs(parsed, form);
    const outerDir = await stageRelease(form, generated);
    await packArchive(outerDir, form.code, form.format, makereleaseRoot);
    await finalizeRelease(outerDir, form.code, makereleaseRoot);
    return {
      archivePath: path.join(makereleaseRoot, `UP-${form.code.toUpperCase()}.${form.format}`),
      releaseNumber: generated.releaseNumber,
    };
  });
}
