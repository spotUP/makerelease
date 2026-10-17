import type { ReleaseHistoryEntry } from '../shared/types';
import { divisionIdOf, type GroupProfile } from '../shared/profile';
import { divisionOf, formatReleaseNumber } from '../shared/divisions';
import { archiveFileName } from '../shared/naming';
import {
  checkCodeExists,
  clientConfigFor,
  deleteRelease,
  fetchManifest,
  fetchNumberingCheck,
  fetchSeed,
  publishRelease,
  publishToWeb,
} from './releases-client';
import { describeStaleDrafts } from './stale-drafts';
import { listArtFromBanner } from './nfo-x-innovations';
import { submitDoor } from './door-repo-client';
import type { ReleaseBackend } from './backend';

/**
 * The Up Rough releases server: it numbers releases, keeps the catalogue and
 * the member roster, publishes to the website, and (for /X) the door
 * repository takes every real build.
 */
export function doorserverBackend(profile: GroupProfile, env: NodeJS.ProcessEnv): ReleaseBackend {
  const division = divisionIdOf(profile.renderer);
  if (!division) {
    // parseProfile already refuses this combination; this guards a profile
    // built some other way.
    throw new Error(`${profile.id}: the release server needs renderer "uprough" or "xinnovations"`);
  }
  const config = clientConfigFor(profile, env);

  return {
    async seed() {
      // Both in one round trip's worth of time, and a numbering check that
      // fails for any reason must never stop the editor from opening - it is
      // a warning, not a precondition.
      const [seed, numbering] = await Promise.all([
        fetchSeed(config, division),
        fetchNumberingCheck(config).catch(() => null),
      ]);
      const numberingWarning =
        numbering && numbering.checked && numbering.ok === false && numbering.summary
          ? `${numbering.summary} Do not build until this is reconciled - the release number goes into the NFO.`
          : undefined;
      const staleDraftWarning = describeStaleDrafts(seed.drafts ?? [], Math.floor(Date.now() / 1000));
      return {
        members: seed.members,
        boards: seed.boards ?? [],
        nextReleaseNumber: seed.nextReleaseNumber,
        history: seed.history,
        currentDizText: seed.currentDizText ?? '',
        revision: seed.revision,
        ...(numberingWarning ? { numberingWarning } : {}),
        ...(staleDraftWarning ? { staleDraftWarning } : {}),
        ...(config.writeKey
          ? {}
          : {
              configWarning:
                `The "${profile.id}" profile has no server write key - publishing a release will fail. ` +
                'Put the key in its profile.json and press Reload profiles.',
            }),
      };
    },

    codeExists: (code) => checkCodeExists(config, code),

    async create(form) {
      const published = await publishRelease(config, form);
      return { releaseNumber: published.releaseNumber, history: published.history };
    },

    async preview(form) {
      // No POST to the server: no real release number consumed, no catalog
      // write, no member-roster replacement. nextReleaseNumber is a preview
      // only - a real build racing this one would still get the same
      // number, since a dry run makes no reservation.
      const seed = await fetchSeed(config, division);
      const releaseNumber = seed.nextReleaseNumber;
      // Synthesize the entry a real publish would have inserted, so the
      // generated NFO's release-history section previews correctly - the
      // filename convention matches catalog.ts's filenamesFor exactly.
      const entry: ReleaseHistoryEntry = {
        releaseNumber: String(releaseNumber),
        filenames: archiveFileName(profile, form.code, form.format).toLowerCase(),
        title: form.entryTitle,
        category: form.entryCategory,
        creators: form.entryCreators,
      };
      return { releaseNumber, history: [entry, ...seed.history] };
    },

    async releaseList(form, releaseNumber) {
      // The /X NFO lists every previous release as its own banner art, which
      // only the manifest carries; its own entry is prepended because a
      // release that is still a draft is not in anyone's history yet.
      if (division !== 'xinnovations') return [];
      const manifest = await fetchManifest(config, division);
      return [
        { displayNumber: formatReleaseNumber(releaseNumber, division), art: listArtFromBanner(form.dizBannerText) },
        ...manifest.history.map((entry) => ({
          displayNumber: entry.releaseNumber,
          art: listArtFromBanner(entry.dizText),
        })),
      ];
    },

    publishToWeb: (code, archivePath, imagePath, embedImagePaths) =>
      publishToWeb(config, code, archivePath, imagePath, embedImagePaths),

    async deleteRelease(code) {
      const result = await deleteRelease(config, code);
      return {
        code: result.code,
        releaseNumber: result.releaseNumber,
        wasPublished: result.wasPublished,
        wasDraft: result.wasDraft,
        wordpressPostUrl: result.wordpressPostUrl,
        archiveRemoved: result.archiveRemoved,
        wordpressPostTrashed: result.wordpressPostTrashed,
        wordpressPostError: result.wordpressPostError,
      };
    },

    async afterBuild(archivePath) {
      // A /X release is an AmiExpress door, so it goes to the door
      // repository too. The build has already succeeded by this point, so a
      // submission that fails is reported, never thrown.
      return divisionOf(division).submitsToDoorRepo ? submitDoor(archivePath) : undefined;
    },
  };
}
