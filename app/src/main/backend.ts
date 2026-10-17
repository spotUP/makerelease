import type {
  DeleteReleaseOutcome,
  DeleteReleaseResult,
  DoorSubmissionOutcome,
  PublishToWebOutcome,
  PublishToWebResult,
  ReleaseFormData,
  ReleaseHistoryEntry,
  SeedState,
} from '../shared/types';
import type { GroupProfile } from '../shared/profile';
import { ReleasesApiError } from './releases-client';
import { doorserverBackend } from './backend-doorserver';
import { noneBackend } from './backend-none';

/** What a build needs from the backend before the NFO can be written. */
export interface NumberedHistory {
  releaseNumber: number;
  history: ReleaseHistoryEntry[];
}

/** One previous release as the /X NFO lists it: its number and its banner art. */
export interface ReleaseListEntry {
  displayNumber: string;
  art: string;
}

/**
 * Everything a release does beyond the files on disk: numbering, history,
 * the catalogue, the website and the door repository. The IPC handlers only
 * stage, pack and finalise; whatever needs a server goes through here, so a
 * group without one simply gets the backend that does nothing.
 */
export interface ReleaseBackend {
  seed(): Promise<Omit<SeedState, 'profile' | 'outputRoot'>>;
  codeExists(code: string): Promise<boolean>;
  /** Real build: reserve a number and return the history the NFO prints. */
  create(form: ReleaseFormData): Promise<NumberedHistory>;
  /** Dry run: same shape, nothing reserved. */
  preview(form: ReleaseFormData): Promise<NumberedHistory>;
  releaseList(form: ReleaseFormData, releaseNumber: number): Promise<ReadonlyArray<ReleaseListEntry>>;
  publishToWeb(code: string, archivePath: string, imagePath: string, embedImagePaths: string[]): Promise<PublishToWebResult>;
  deleteRelease(code: string): Promise<DeleteReleaseResult>;
  /** Runs after a real build only; a dry run never reaches anyone's queue. */
  afterBuild(archivePath: string): Promise<DoorSubmissionOutcome | undefined>;
}

export function backendFor(profile: GroupProfile, env: NodeJS.ProcessEnv): ReleaseBackend {
  return profile.backend === 'releases-doorserver' ? doorserverBackend(profile, env) : noneBackend(profile);
}

function messageOf(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

/**
 * Turns a failed website publish into the outcome the window shows. A plain
 * returned object rather than a throw: Electron prepends
 * `Error invoking remote method '<channel>': Error: ` to a thrown Error's
 * message and strips its custom properties on the way to the renderer.
 */
export function publishFailure(err: unknown): PublishToWebOutcome {
  if (err instanceof ReleasesApiError) {
    return {
      ok: false,
      message: err.message,
      status: err.status,
      reason: err.reason,
      wordpressPostUrl: err.wordpressPostUrl,
    };
  }
  return { ok: false, message: messageOf(err) };
}

/** Same as publishFailure, for a revoke. */
export function deleteFailure(err: unknown): DeleteReleaseOutcome {
  if (err instanceof ReleasesApiError) return { ok: false, message: err.message, status: err.status };
  return { ok: false, message: messageOf(err) };
}
