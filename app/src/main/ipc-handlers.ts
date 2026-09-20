import { dialog, ipcMain } from 'electron';
import * as path from 'path';
import { BuildResult, DeleteReleaseOutcome, PublishToWebOutcome, ReleaseFormData, ReleaseHistoryEntry, SeedState } from '../shared/types';
import {
  fetchSeed,
  fetchNumberingCheck,
  checkCodeExists,
  publishRelease,
  deleteRelease as deleteReleaseClient,
  publishToWeb as publishToWebClient,
  ReleasesApiError,
} from './releases-client';
import { describeStaleDrafts } from './stale-drafts';
import { generateOutputs } from './nfo-generator';
import { stageRelease } from './staging';
import { packArchive, finalizeRelease } from './archiver';
import { pathExists, ensureDir, removePath } from './fs-helpers';
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

/**
 * Where a dry run writes, under the output root.
 *
 * A dry run produces a real archive so it can be opened and inspected, and it
 * used to write it exactly where a real build lands. That made the obvious
 * workflow - dry run, look inside, then build for real - fail every time:
 * assertLocalCodeAvailable saw the preview and could not tell it apart from
 * the leftovers of a failed attempt, which is the thing it exists to catch.
 * Giving previews their own folder keeps that guard's meaning intact.
 */
export const DRY_RUN_DIR = 'dry-run';

export function registerIpcHandlers(makereleaseRoot: string, releasesConfig: ReleasesClientConfig): void {
  ipcMain.handle('loadSeedState', async (): Promise<SeedState> => {
    // Both in one round trip's worth of time, and a numbering check that
    // fails for any reason must never stop the editor from opening - it is
    // a warning, not a precondition.
    const [seed, numbering] = await Promise.all([
      fetchSeed(releasesConfig),
      fetchNumberingCheck(releasesConfig).catch(() => null),
    ]);
    const numberingWarning =
      numbering && numbering.checked && numbering.ok === false && numbering.summary
        ? `${numbering.summary} Do not build until this is reconciled - the release number goes into the NFO.`
        : undefined;
    const staleDraftWarning = describeStaleDrafts(seed.drafts ?? [], Math.floor(Date.now() / 1000));
    return {
      members: seed.members,
      nextReleaseNumber: seed.nextReleaseNumber,
      history: seed.history,
      currentDizText: seed.currentDizText ?? '',
      revision: seed.revision,
      ...(numberingWarning ? { numberingWarning } : {}),
      ...(staleDraftWarning ? { staleDraftWarning } : {}),
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

  ipcMain.handle('buildRelease', async (_event, form: ReleaseFormData, dryRun: boolean): Promise<BuildResult> => {
    const upperCode = form.code.toUpperCase();
    const outputRoot = dryRun ? path.join(makereleaseRoot, DRY_RUN_DIR) : makereleaseRoot;
    if (dryRun) {
      // A preview is disposable by definition, so the newest one replaces
      // the last instead of refusing to overwrite it.
      await ensureDir(outputRoot);
      await removePath(path.join(outputRoot, `UP-${upperCode}`));
      await removePath(path.join(outputRoot, `UP-${upperCode}.${form.format}`));
    } else {
      await assertLocalCodeAvailable(makereleaseRoot, form.code);
    }

    let releaseNumber: number;
    let history: ReleaseHistoryEntry[];
    if (dryRun) {
      // No POST to the server: no real release number consumed, no catalog
      // write, no member-roster replacement. nextReleaseNumber is a preview
      // only - a real build racing this one would still get the same
      // number, since dry-run makes no reservation.
      const seed = await fetchSeed(releasesConfig);
      releaseNumber = seed.nextReleaseNumber;
      // Synthesize the entry a real publish would have inserted, so the
      // generated NFO's release-history section previews correctly - the
      // filename convention matches catalog.ts's filenamesFor exactly.
      const dryRunEntry: ReleaseHistoryEntry = {
        releaseNumber: String(releaseNumber),
        filenames: `up-${form.code.toLowerCase()}.${form.format.toLowerCase()}`,
        title: form.entryTitle,
        category: form.entryCategory,
        creators: form.entryCreators,
      };
      history = [dryRunEntry, ...seed.history];
    } else {
      const published = await publishRelease(releasesConfig, form);
      releaseNumber = published.releaseNumber;
      history = published.history;
    }

    const generated = generateOutputs(history, releaseNumber, form);
    const outerDir = await stageRelease(form, generated);
    await packArchive(outerDir, form.code, form.format, outputRoot);
    await finalizeRelease(outerDir, form.code, outputRoot);
    return {
      archivePath: path.join(outputRoot, `UP-${upperCode}.${form.format}`),
      releaseNumber,
      isDryRun: dryRun,
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
    async (
      _event,
      code: string,
      archivePath: string,
      imagePath: string,
      embedImagePaths: string[],
    ): Promise<PublishToWebOutcome> => {
      try {
        const result = await publishToWebClient(releasesConfig, code, archivePath, imagePath, embedImagePaths);
        return { ok: true, ...result };
      } catch (err) {
        // Return a plain discriminated result rather than throwing: Electron
        // prepends `Error invoking remote method '<channel>': Error: ` to a
        // thrown Error's message before the renderer sees it, and strips
        // every custom property via structured-clone. A plain returned
        // object crosses the IPC boundary intact.
        if (err instanceof ReleasesApiError) {
          return {
            ok: false,
            message: err.message,
            status: err.status,
            reason: err.reason,
            wordpressPostUrl: err.wordpressPostUrl,
          };
        }
        return { ok: false, message: err instanceof Error ? err.message : String(err) };
      }
    },
  );

  ipcMain.handle('deleteRelease', async (_event, code: string): Promise<DeleteReleaseOutcome> => {
    try {
      const result = await deleteReleaseClient(releasesConfig, code);
      return {
        ok: true,
        code: result.code,
        releaseNumber: result.releaseNumber,
        wasPublished: result.wasPublished,
        wasDraft: result.wasDraft,
        wordpressPostUrl: result.wordpressPostUrl,
        archiveRemoved: result.archiveRemoved,
        wordpressPostTrashed: result.wordpressPostTrashed,
        wordpressPostError: result.wordpressPostError,
      };
    } catch (err) {
      if (err instanceof ReleasesApiError) {
        return { ok: false, message: err.message, status: err.status };
      }
      return { ok: false, message: err instanceof Error ? err.message : String(err) };
    }
  });
}
