import { dialog, ipcMain, shell } from 'electron';
import * as path from 'path';
import {
  AminetUploadRequest,
  BuildResult,
  DeleteReleaseOutcome,
  PublishToWebOutcome,
  ReleaseFormData,
  ReleaseHistoryEntry,
  SeedState,
} from '../shared/types';
import {
  fetchSeed,
  fetchManifest,
  fetchNumberingCheck,
  checkCodeExists,
  publishRelease,
  deleteRelease as deleteReleaseClient,
  publishToWeb as publishToWebClient,
  ReleasesApiError,
  clientConfigFor,
} from './releases-client';
import { divisionOf, formatReleaseNumber } from '../shared/divisions';
import { divisionIdOf, summarize, type GroupProfile, type ProfileListEntry } from '../shared/profile';
import { archiveFileName, releaseBaseName } from '../shared/naming';
import { isProfileLoadError, listProfiles, loadProfile } from './profiles';
import { describeStaleDrafts } from './stale-drafts';
import { generateOutputs } from './nfo-generator';
import { listArtFromBanner } from './nfo-x-innovations';
import { submitDoor } from './door-repo-client';
import { uploadToAminet } from './aminet';
import { stageRelease } from './staging';
import { packArchive, finalizeRelease } from './archiver';
import { pathExists, ensureDir, removePath } from './fs-helpers';

// Local pre-flight check, purely filesystem-based (no network call). Guards
// buildRelease against burning a server-side release number (publishRelease
// atomically assigns a number AND replaces the entire member roster) for a
// build that was always going to fail locally, e.g. because a previous
// attempt left a stale <PREFIX><CODE> directory or archive behind. This mirrors
// what the pre-migration repo-scanner.ts's codeExists used to check before
// any server call existed. It is intentionally separate from the
// checkCodeAvailability IPC handler, which queries the server for
// live-typing UI feedback and must keep doing that.
async function assertLocalCodeAvailable(profile: GroupProfile, outputRoot: string, code: string): Promise<void> {
  const releaseDir = releaseBaseName(profile, code);
  const candidates = [
    path.join(outputRoot, releaseDir),
    path.join(outputRoot, archiveFileName(profile, code, 'LHA')),
    path.join(outputRoot, archiveFileName(profile, code, 'ZIP')),
  ];
  for (const candidate of candidates) {
    if (await pathExists(candidate)) {
      throw new Error(
        `${releaseDir} already exists locally at ${candidate} — refusing to build. ` +
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

/** Where a profile's builds land; packaged and development builds differ. */
export type OutputRootFor = (profile: GroupProfile) => string;

/**
 * Every handler names its profile by id and loads it from disk on each call,
 * so an edit to profile.json followed by "Reload profiles" takes effect
 * without a restart, and the output root follows the profile chosen in the
 * window rather than one fixed at startup.
 */
export function registerIpcHandlers(
  profilesDir: string,
  outputRootFor: OutputRootFor,
  env: NodeJS.ProcessEnv = process.env,
): void {
  ipcMain.handle('listProfiles', async (): Promise<ProfileListEntry[]> => {
    const entries = await listProfiles(profilesDir);
    return entries.map((entry) => (isProfileLoadError(entry) ? entry : summarize(entry)));
  });

  ipcMain.handle('openProfilesFolder', async (): Promise<void> => {
    await ensureDir(profilesDir);
    // shell.openPath reports failure as a non-empty string rather than a throw.
    const problem = await shell.openPath(profilesDir);
    if (problem) throw new Error(`Could not open ${profilesDir}: ${problem}`);
  });

  ipcMain.handle('loadSeedState', async (_event, profileId: string): Promise<SeedState> => {
    const profile = await loadProfile(profilesDir, profileId);
    const base = { profile: summarize(profile), outputRoot: outputRootFor(profile) };
    const division = divisionIdOf(profile.renderer);
    if (profile.backend === 'none' || !division) {
      // No server: nothing is numbered and nothing is remembered between
      // releases, so the profile's own member list is the whole seed.
      return {
        ...base,
        members: profile.members,
        boards: [],
        nextReleaseNumber: 0,
        history: [],
        currentDizText: '',
        revision: 0,
      };
    }
    const releasesConfig = clientConfigFor(profile, env);
    // Both in one round trip's worth of time, and a numbering check that
    // fails for any reason must never stop the editor from opening - it is
    // a warning, not a precondition.
    const [seed, numbering] = await Promise.all([
      fetchSeed(releasesConfig, division),
      fetchNumberingCheck(releasesConfig).catch(() => null),
    ]);
    const numberingWarning =
      numbering && numbering.checked && numbering.ok === false && numbering.summary
        ? `${numbering.summary} Do not build until this is reconciled - the release number goes into the NFO.`
        : undefined;
    const staleDraftWarning = describeStaleDrafts(seed.drafts ?? [], Math.floor(Date.now() / 1000));
    return {
      ...base,
      members: seed.members,
      boards: seed.boards ?? [],
      nextReleaseNumber: seed.nextReleaseNumber,
      history: seed.history,
      currentDizText: seed.currentDizText ?? '',
      revision: seed.revision,
      ...(numberingWarning ? { numberingWarning } : {}),
      ...(staleDraftWarning ? { staleDraftWarning } : {}),
      ...(releasesConfig.writeKey
        ? {}
        : {
            configWarning:
              `The "${profile.id}" profile has no server write key - publishing a release will fail. ` +
              'Put the key in its profile.json and press Reload profiles.',
          }),
    };
  });

  ipcMain.handle('pickFilesAndDirs', async (): Promise<string[]> => {
    const result = await dialog.showOpenDialog({
      properties: ['openFile', 'openDirectory', 'multiSelections'],
    });
    if (result.canceled) return [];
    return result.filePaths;
  });

  ipcMain.handle(
    'checkCodeAvailability',
    async (_event, profileId: string, code: string): Promise<{ exists: boolean }> => {
      const profile = await loadProfile(profilesDir, profileId);
      // Without a server there is no catalogue to collide with; the local
      // pre-flight in buildRelease still refuses to overwrite a leftover.
      if (profile.backend === 'none') return { exists: false };
      return { exists: await checkCodeExists(clientConfigFor(profile, env), code) };
    },
  );

  ipcMain.handle('buildRelease', async (_event, sentForm: ReleaseFormData, dryRun: boolean): Promise<BuildResult> => {
    const profile = await loadProfile(profilesDir, sentForm.profileId);
    const division = divisionIdOf(profile.renderer) ?? undefined;
    // The division follows from the profile, never from what the window sent.
    const { division: _sentDivision, ...rest } = sentForm;
    const form: ReleaseFormData = { ...rest, ...(division ? { division } : {}) };
    const profileRoot = outputRootFor(profile);
    const outputRoot = dryRun ? path.join(profileRoot, DRY_RUN_DIR) : profileRoot;
    await ensureDir(outputRoot);
    if (dryRun) {
      // A preview is disposable by definition, so the newest one replaces
      // the last instead of refusing to overwrite it.
      await removePath(path.join(outputRoot, releaseBaseName(profile, form.code)));
      await removePath(path.join(outputRoot, archiveFileName(profile, form.code, form.format)));
    } else {
      await assertLocalCodeAvailable(profile, profileRoot, form.code);
    }
    const releasesConfig = profile.backend === 'releases-doorserver' ? clientConfigFor(profile, env) : null;

    let releaseNumber: number;
    let history: ReleaseHistoryEntry[];
    if (!releasesConfig || !division) {
      // No server: nothing to reserve, nothing to remember.
      releaseNumber = 0;
      history = [];
    } else if (dryRun) {
      // No POST to the server: no real release number consumed, no catalog
      // write, no member-roster replacement. nextReleaseNumber is a preview
      // only - a real build racing this one would still get the same
      // number, since dry-run makes no reservation.
      const seed = await fetchSeed(releasesConfig, division);
      releaseNumber = seed.nextReleaseNumber;
      // Synthesize the entry a real publish would have inserted, so the
      // generated NFO's release-history section previews correctly - the
      // filename convention matches catalog.ts's filenamesFor exactly.
      const dryRunEntry: ReleaseHistoryEntry = {
        releaseNumber: String(releaseNumber),
        filenames: archiveFileName(profile, form.code, form.format).toLowerCase(),
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

    // The /X NFO lists every previous release as its own banner art, which
    // only the manifest carries; its own entry is prepended because a
    // release that is still a draft is not in anyone's history yet.
    const releaseList =
      releasesConfig && division === 'xinnovations'
        ? [
            { displayNumber: formatReleaseNumber(releaseNumber, division), art: listArtFromBanner(form.dizBannerText) },
            ...(await fetchManifest(releasesConfig, division)).history.map((entry) => ({
              displayNumber: entry.releaseNumber,
              art: listArtFromBanner(entry.dizText),
            })),
          ]
        : [];

    const generated = generateOutputs(profile, history, releaseNumber, form, releaseList);
    const outerDir = await stageRelease(profile, form, generated);
    await packArchive(profile, outerDir, form.code, form.format, outputRoot);
    await finalizeRelease(profile, outerDir, form.code, outputRoot);
    const archivePath = path.join(outputRoot, archiveFileName(profile, form.code, form.format));

    // A /X release is an AmiExpress door, so it goes to the door repository
    // too - but only for real: a dry run must not put a preview in anyone's
    // queue. The build has already succeeded by this point, so a submission
    // that fails is reported, never thrown.
    const submitsToDoorRepo = releasesConfig !== null && division !== undefined && divisionOf(division).submitsToDoorRepo;
    const doorSubmission = !dryRun && submitsToDoorRepo ? await submitDoor(archivePath) : undefined;

    return {
      archivePath,
      releaseNumber,
      isDryRun: dryRun,
      ...(doorSubmission ? { doorSubmission } : {}),
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
      profileId: string,
      code: string,
      archivePath: string,
      imagePath: string,
      embedImagePaths: string[],
      aminet: AminetUploadRequest | null = null,
    ): Promise<PublishToWebOutcome> => {
      try {
        const profile = await loadProfile(profilesDir, profileId);
        const result = await publishToWebClient(clientConfigFor(profile, env), code, archivePath, imagePath, embedImagePaths);
        // Aminet only after the website took the release: a publish the
        // server refused (409, number taken) must not leave a copy on a
        // public archive nobody can revoke from here. The Aminet result is
        // reported alongside, never thrown - the website publish succeeded.
        const aminetUpload = aminet ? await uploadToAminet(aminet, archivePath) : undefined;
        return { ok: true, ...result, ...(aminetUpload ? { aminetUpload } : {}) };
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

  ipcMain.handle('deleteRelease', async (_event, profileId: string, code: string): Promise<DeleteReleaseOutcome> => {
    try {
      const profile = await loadProfile(profilesDir, profileId);
      const result = await deleteReleaseClient(clientConfigFor(profile, env), code);
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
