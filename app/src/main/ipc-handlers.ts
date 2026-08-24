import { dialog, ipcMain } from 'electron';
import * as path from 'path';
import { BuildResult, PublishToWebOutcome, ReleaseFormData, SeedState } from '../shared/types';
import {
  fetchSeed,
  checkCodeExists,
  publishRelease,
  publishToWeb as publishToWebClient,
  ReleasesApiError,
} from './releases-client';
import { generateOutputs } from './nfo-generator';
import { stageRelease } from './staging';
import { packArchive, finalizeRelease } from './archiver';
import { pathExists } from './fs-helpers';
import { ReleasesClientConfig } from './config';

// Local pre-flight check, purely filesystem-based (no network call). Guards
// buildRelease against burning a server-side release number (publishRelease
// atomically assigns a number AND replaces the entire member roster) for a
// build that was always going to fail locally, e.g. because a previous
// attempt left a stale UP-<CODE> directory or archive behind. This mirrors
// what the pre-migration repo-scanner.ts's codeExists used to check before
// any server call existed. It is intentionally separate from the
// checkCodeAvailability IPC handler, which queries the server for
// live-typing UI feedback and must keep doing that.
async function assertLocalCodeAvailable(makereleaseRoot: string, code: string): Promise<void> {
  const upperCode = code.toUpperCase();
  const candidates = [
    path.join(makereleaseRoot, `UP-${upperCode}`),
    path.join(makereleaseRoot, `UP-${upperCode}.LHA`),
    path.join(makereleaseRoot, `UP-${upperCode}.ZIP`),
  ];
  for (const candidate of candidates) {
    if (await pathExists(candidate)) {
      throw new Error(
        `UP-${upperCode} already exists locally at ${candidate} — refusing to build. ` +
          'Remove or rename the leftover file/folder from a previous attempt, or choose a different code.',
      );
    }
  }
}

export function registerIpcHandlers(makereleaseRoot: string, releasesConfig: ReleasesClientConfig): void {
  ipcMain.handle('loadSeedState', async (): Promise<SeedState> => {
    const seed = await fetchSeed(releasesConfig);
    return {
      members: seed.members,
      nextReleaseNumber: seed.nextReleaseNumber,
      history: seed.history,
      currentDizText: seed.currentDizText ?? '',
      revision: seed.revision,
      ...(releasesConfig.isPlaceholderKey
        ? {
            configWarning:
              'This build has no RELEASES_WRITE_KEY baked in — publishing a release will fail. ' +
              'Contact the maintainer for a properly configured build.',
          }
        : {}),
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
    await assertLocalCodeAvailable(makereleaseRoot, form.code);
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

  ipcMain.handle('pickImage', async (): Promise<string | null> => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile'],
      filters: [{ name: 'Images', extensions: ['png', 'jpg', 'jpeg', 'gif'] }],
    });
    if (result.canceled || result.filePaths.length === 0) return null;
    return result.filePaths[0];
  });

  ipcMain.handle(
    'publishToWeb',
    async (_event, code: string, archivePath: string, imagePath: string): Promise<PublishToWebOutcome> => {
      try {
        const result = await publishToWebClient(releasesConfig, code, archivePath, imagePath);
        return { ok: true, ...result };
      } catch (err) {
        // Return a plain discriminated result rather than throwing: Electron
        // prepends `Error invoking remote method '<channel>': Error: ` to a
        // thrown Error's message before the renderer sees it, and strips
        // every custom property via structured-clone. A plain returned
        // object crosses the IPC boundary intact.
        if (err instanceof ReleasesApiError) {
          return { ok: false, message: err.message, status: err.status, wordpressPostUrl: err.wordpressPostUrl };
        }
        return { ok: false, message: err instanceof Error ? err.message : String(err) };
      }
    },
  );
}
