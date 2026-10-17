import { dialog, ipcMain, shell } from 'electron';
import * as path from 'path';
import {
  AminetUploadOutcome,
  AminetUploadRequest,
  BuildResult,
  OutputsPreview,
  DeleteReleaseOutcome,
  PublishToWebOutcome,
  ReleaseFormData,
  SeedState,
} from '../shared/types';
import { divisionIdOf, summarize, type GroupProfile, type ProfileListEntry } from '../shared/profile';
import { archiveFileName, releaseBaseName } from '../shared/naming';
import { isProfileLoadError, listProfiles, loadProfile, readTemplates } from './profiles';
import { backendFor, deleteFailure, publishFailure } from './backend';
import { generateOutputs } from './nfo-generator';
import { placeholderProblems, renderPlaceholderOutputs } from './nfo-placeholder';
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
    const seed = await backendFor(profile, env).seed();
    return { profile: summarize(profile), outputRoot: outputRootFor(profile), ...seed };
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
      return { exists: await backendFor(profile, env).codeExists(code) };
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
    const backend = backendFor(profile, env);
    const { releaseNumber, history } = dryRun ? await backend.preview(form) : await backend.create(form);
    const releaseList = await backend.releaseList(form, releaseNumber);

    const templates = profile.renderer === 'placeholder' ? await readTemplates(profilesDir, profile.id) : undefined;
    const generated = generateOutputs(profile, history, releaseNumber, form, releaseList, templates);
    const outerDir = await stageRelease(profile, form, generated);
    await packArchive(profile, outerDir, form.code, form.format, outputRoot);
    await finalizeRelease(profile, outerDir, form.code, outputRoot);
    const archivePath = path.join(outputRoot, archiveFileName(profile, form.code, form.format));

    const doorSubmission = dryRun ? undefined : await backend.afterBuild(archivePath);

    return {
      archivePath,
      releaseNumber,
      isDryRun: dryRun,
      ...(doorSubmission ? { doorSubmission } : {}),
    };
  });

  // Template files are re-read on every call, so an edit to release.nfo or
  // file_id.diz shows up in the window without reloading anything.
  ipcMain.handle('previewOutputs', async (_event, form: ReleaseFormData): Promise<OutputsPreview | null> => {
    const profile = await loadProfile(profilesDir, form.profileId);
    if (profile.renderer !== 'placeholder') return null;
    const templates = await readTemplates(profilesDir, profile.id);
    const problems = placeholderProblems(profile, templates, form);
    const hasNfoTemplate = templates.nfo !== null;
    if (problems.length) {
      return { dizText: '', nfoText: null, hasNfoTemplate, problems };
    }
    const { dizText, nfoText } = renderPlaceholderOutputs(profile, templates, form);
    return { dizText, nfoText, hasNfoTemplate, problems };
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
    ): Promise<PublishToWebOutcome> => {
      try {
        const profile = await loadProfile(profilesDir, profileId);
        const result = await backendFor(profile, env).publishToWeb(code, archivePath, imagePath, embedImagePaths);
        return { ok: true, ...result };
      } catch (err) {
        return publishFailure(err);
      }
    },
  );

  // Aminet is its own step, not part of the website publish: a group without
  // a website still uploads, and the renderer calls this only after a
  // website publish succeeded when there is one, so a release the server
  // refused never lands on a public archive nobody can revoke from here.
  ipcMain.handle(
    'uploadToAminet',
    async (_event, request: AminetUploadRequest, archivePath: string): Promise<AminetUploadOutcome> =>
      uploadToAminet(request, archivePath),
  );

  ipcMain.handle('deleteRelease', async (_event, profileId: string, code: string): Promise<DeleteReleaseOutcome> => {
    try {
      const profile = await loadProfile(profilesDir, profileId);
      return { ok: true, ...(await backendFor(profile, env).deleteRelease(code)) };
    } catch (err) {
      return deleteFailure(err);
    }
  });
}
