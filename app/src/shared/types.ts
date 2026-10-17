import type { DivisionId } from './divisions';
import type { GroupProfile, ProfileListEntry, ProfileSummary } from './profile';

export interface MemberEntry {
  nickname: string;
  role: string;
}

export interface ReleaseHistoryEntry {
  releaseNumber: string;
  filenames: string;
  title: string;
  category: string;
  creators: string;
}

export type ReleaseFormat = 'LHA' | 'ZIP';

export interface CreditEntry {
  role: string;
  name: string;
}

// What's sent to the server at build time (POST /api/releases). An 'image'
// entry carries no path here - the server only knows it by position among
// the release's stored image-type embeds. The actual local file path lives
// only in the renderer's own MediaEmbedsList state, captured separately at
// build-success time for the later Publish step (see main.ts's
// lastEmbedImagePaths) - never part of the wire format sent to the server.
export type MediaEmbedFormEntry =
  | { type: 'youtube'; caption: string; youtubeUrl: string }
  | { type: 'image'; caption: string };

export interface ReleaseFormData {
  /** The group profile this release is built for (its folder name). */
  profileId: string;
  /**
   * The code renderer's division. Set by main from the profile on every
   * build; whatever the renderer sends is overwritten. Absent for the
   * placeholder renderer.
   */
  division?: DivisionId;
  /** Release number typed by hand, for groups without a numbering server. */
  releaseNumberText?: string;
  code: string;
  format: ReleaseFormat;
  releaseName: string;
  platformTag: string;
  releaseNotesText: string;
  newsText: string;
  dizBannerText: string;
  members: MemberEntry[];
  entryTitle: string;
  entryCategory: string;
  entryCreators: string;
  selectedPaths: string[];
  description: string;
  credits: CreditEntry[];
  mediaEmbeds: MediaEmbedFormEntry[];
  /** /X only: the boards its NFO lists. Omitted leaves the stored list alone. */
  boards?: BoardEntry[];
  /** /X only: the bracketed fields its NFO prints under the logo. */
  releaseDate?: string;
  coder?: string;
  system?: string;
  design?: string;
}

export interface GeneratedOutputs {
  /** null: a placeholder profile without a release.nfo template ships no NFO. */
  nfoText: string | null;
  dizText: string;
  releaseNumber: number;
}

export interface BoardEntry {
  name: string;
  sysop: string;
  hq: string;
  status: string;
}

export interface SeedState {
  /** The profile this seed was loaded for. */
  profile: ProfileSummary;
  /** Where this profile's builds land; resolved per profile, not at startup. */
  outputRoot: string;
  members: MemberEntry[];
  /** Boards the division lists in its NFO; empty for Up Rough. */
  boards: BoardEntry[];
  nextReleaseNumber: number;
  history: ReleaseHistoryEntry[];
  currentDizText: string;
  revision: number;
  /**
   * Set when the profile's server block has an empty writeKey and no
   * RELEASES_WRITE_KEY override is set. Publishing a release will 401. The
   * renderer surfaces this to the user; see main.ts's init().
   */
  configWarning?: string;
  /**
   * Set when the catalogue's release numbering no longer matches
   * www.uprough.net - a release posted to the site by hand holds a number
   * this tool never spent, so every number the tool hands out afterwards is
   * one too low. The build itself is refused server-side once the next
   * number is one the site already used; this warning is what makes the
   * drift visible BEFORE anyone fills in a form. See the releases server's
   * GET /api/releases/numbering-check.
   */
  numberingWarning?: string;
  /**
   * Set when a previous build was never published and never revoked. Such a
   * build is invisible in the catalogue but keeps its release number and its
   * code forever - an abandoned Evoke build is exactly what left 217 spent
   * while the catalogue looked like it ended at 216. See
   * describeStaleDrafts in main/stale-drafts.ts.
   */
  staleDraftWarning?: string;
}

export interface DoorSubmissionOutcome {
  ok: boolean;
  message: string;
  id?: number;
}

/** What the window shows while a placeholder profile's form is filled in. */
export interface OutputsPreview {
  dizText: string;
  /** null when the profile has no release.nfo, or when a template problem stops rendering. */
  nfoText: string | null;
  hasNfoTemplate: boolean;
  /** Each as "<file> line N: <problem>"; empty when both templates render. */
  problems: string[];
}

export interface BuildResult {
  archivePath: string;
  releaseNumber: number;
  /**
   * True when this build ran with dryRun: no POST to the server, no real
   * release number consumed, no catalog write. releaseNumber is a preview
   * (the seed's nextReleaseNumber at build time), not a reserved one. The
   * renderer must not offer Publish for a dry-run build — there is no
   * server-side release row to publish.
   */
  isDryRun: boolean;
  /**
   * Set when the division sends its releases to the AmiExpress door
   * repository. A failure here never fails the build - the archive exists
   * and the release is already in the catalogue - so the outcome is
   * reported rather than thrown.
   */
  doorSubmission?: DoorSubmissionOutcome;
}

/**
 * What the renderer sends to upload a built release to Aminet. The readme text is the exact text the renderer previewed;
 * main normalises and re-checks it but does not compose it, so what the user
 * saw is what Aminet gets.
 */
export interface AminetUploadRequest {
  readmeText: string;
  /** Doubles as the anonymous-FTP password, as Aminet asks. */
  uploaderEmail: string;
}

/**
 * Result of the Aminet upload. Never a thrown error: Aminet is a courtesy
 * mirror, and a failure there must not make a successful website publish
 * look failed. `readmePath` is where the .readme was written (next to the
 * archive), so a failed upload can be redone by hand.
 */
export interface AminetUploadOutcome {
  ok: boolean;
  message: string;
  readmePath?: string;
}

export interface PublishToWebResult {
  archiveUrl: string;
  wordpressPostUrl: string;
}

/**
 * publishToWeb's IPC result is a discriminated union rather than a thrown
 * Error, because Electron's ipcMain.handle/ipcRenderer.invoke boundary
 * mangles a thrown Error's message (it prepends
 * `Error invoking remote method '<channel>': Error: ` before the renderer
 * sees it) and strips every custom property via structured-clone. A plain
 * returned object crosses intact. Only a genuinely unexpected failure (a
 * bug, not a modeled outcome like "already published") should still throw.
 */
export type PublishToWebOutcome =
  | ({ ok: true } & PublishToWebResult)
  | { ok: false; message: string; status?: number; reason?: string; wordpressPostUrl?: string };

export interface DeleteReleaseResult {
  code: string;
  releaseNumber: number;
  wasPublished: boolean;
  /**
   * True when the release was still a draft - built but never published to
   * the web, so nobody ever saw it. Revoking one of these is free.
   */
  wasDraft: boolean;
  wordpressPostUrl: string | null;
  archiveRemoved: boolean;
  /**
   * Whether the server moved the WordPress post to the trash (recoverable in
   * wp-admin for 30 days). False when there was no post, or when WordPress
   * refused - `wordpressPostError` then says why and the post needs removing
   * by hand.
   */
  wordpressPostTrashed: boolean;
  wordpressPostError: string | null;
}

// Same discriminated-result shape as PublishToWebOutcome, and for the same
// reason: a thrown Error's message is mangled crossing ipcMain.handle's IPC
// boundary, so any modeled failure (404, 409) is returned, not thrown.
export type DeleteReleaseOutcome =
  | ({ ok: true } & DeleteReleaseResult)
  | { ok: false; message: string; status?: number };

/** A group's NFO (optional) and DIZ template texts, decoded and with LF line endings. */
export interface ProfileTemplatesText {
  nfo: string | null;
  diz: string;
}

/** A profile as the editor dialog holds it: everything but the id, which is the folder name. */
export interface ProfileEditRequest {
  id: string;
  isNew: boolean;
  profile: Omit<GroupProfile, 'id'>;
  /** Omitted for the code renderers, whose template files stay untouched. */
  templates?: ProfileTemplatesText;
}

export interface ProfileEditState {
  profile: GroupProfile;
  templates: ProfileTemplatesText;
}

/** Same shape as the publish outcomes: a refusal is returned, not thrown, so its message survives IPC. */
export type ProfileEditOutcome = { ok: true } | { ok: false; error: string };

export interface MakereleaseApi {
  listProfiles(): Promise<ProfileListEntry[]>;
  openProfilesFolder(): Promise<void>;
  newProfileDraft(): Promise<ProfileEditState>;
  loadProfileForEdit(id: string): Promise<ProfileEditState>;
  saveProfile(request: ProfileEditRequest): Promise<ProfileEditOutcome>;
  deleteProfile(id: string): Promise<ProfileEditOutcome>;
  /** Lets the user pick a template file; null when cancelled. */
  importTemplate(): Promise<{ text: string; name: string } | null>;
  loadSeedState(profileId: string): Promise<SeedState>;
  pickFilesAndDirs(): Promise<string[]>;
  checkCodeAvailability(profileId: string, code: string): Promise<{ exists: boolean }>;
  buildRelease(form: ReleaseFormData, dryRun: boolean): Promise<BuildResult>;
  /** A placeholder profile's NFO and DIZ as they would build now; null for the code renderers. */
  previewOutputs(form: ReleaseFormData): Promise<OutputsPreview | null>;
  pickImage(): Promise<string | null>;
  publishToWeb(
    profileId: string,
    code: string,
    archivePath: string,
    imagePath: string,
    embedImagePaths: string[],
  ): Promise<PublishToWebOutcome>;
  uploadToAminet(request: AminetUploadRequest, archivePath: string): Promise<AminetUploadOutcome>;
  deleteRelease(profileId: string, code: string): Promise<DeleteReleaseOutcome>;
}
