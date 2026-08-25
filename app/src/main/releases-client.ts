import { openAsBlob } from 'fs';
import * as path from 'path';
import { MemberEntry, PublishToWebResult, ReleaseFormData, ReleaseHistoryEntry } from '../shared/types';
import { ReleasesClientConfig } from './config';
import { trimDizBanner } from './nfo-generator';

export class ReleasesApiError extends Error {
  /** HTTP status code of the response that produced this error, when known. */
  status?: number;
  /**
   * The server's machine-readable reason for a 409, when it sent one:
   * 'already-published', 'publish-in-progress', or 'number-taken' (WordPress
   * already has a post claiming this release number). The three need
   * different handling, and sniffing the prose would be fragile.
   */
  reason?: string;
  /**
   * Populated when the server's 409 "already published" response body
   * included the existing WordPress post URL, so callers can show it
   * instead of just the bare error message.
   */
  wordpressPostUrl?: string;

  constructor(message: string, opts?: { status?: number; reason?: string; wordpressPostUrl?: string }) {
    super(message);
    this.status = opts?.status;
    this.reason = opts?.reason;
    this.wordpressPostUrl = opts?.wordpressPostUrl;
  }
}

export interface SeedResponse {
  members: MemberEntry[];
  nextReleaseNumber: number;
  history: ReleaseHistoryEntry[];
  currentDizText: string | null;
  revision: number;
}

export interface PublishResponse {
  releaseNumber: number;
  revision: number;
  history: ReleaseHistoryEntry[];
}

export interface DeleteResponse {
  deleted: true;
  code: string;
  releaseNumber: number;
  wasPublished: boolean;
  wasDraft: boolean;
  wordpressPostUrl: string | null;
  archiveRemoved: boolean;
  wordpressPostTrashed: boolean;
  wordpressPostError: string | null;
}

async function getJson<T>(cfg: ReleasesClientConfig, path: string): Promise<T> {
  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}${path}`, { method: 'GET' });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError(
      `releases.uprough.net returned ${response.status}: ${(body as { error?: string }).error ?? 'unknown error'}`,
    );
  }
  return body as T;
}

export async function fetchSeed(cfg: ReleasesClientConfig): Promise<SeedResponse> {
  return getJson<SeedResponse>(cfg, '/api/releases/seed');
}

export async function checkCodeExists(cfg: ReleasesClientConfig, code: string): Promise<boolean> {
  const result = await getJson<{ exists: boolean }>(cfg, `/api/releases/${encodeURIComponent(code)}/exists`);
  return result.exists;
}

function toPublishBody(form: ReleaseFormData): Record<string, unknown> {
  return {
    code: form.code,
    format: form.format,
    releaseName: form.releaseName,
    platformTag: form.platformTag,
    releaseNotes: form.releaseNotesText,
    news: form.newsText,
    dizText: trimDizBanner(form.dizBannerText),
    entryTitle: form.entryTitle,
    entryCategory: form.entryCategory,
    entryCreators: form.entryCreators,
    members: form.members,
    description: form.description,
    credits: form.credits,
    mediaEmbeds: form.mediaEmbeds,
  };
}

export async function publishRelease(cfg: ReleasesClientConfig, form: ReleaseFormData): Promise<PublishResponse> {
  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}/api/releases`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Release-Key': cfg.writeKey },
      body: JSON.stringify(toPublishBody(form)),
    });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError((body as { error?: string }).error ?? `releases.uprough.net returned ${response.status}`);
  }
  return body as PublishResponse;
}

export async function deleteRelease(cfg: ReleasesClientConfig, code: string): Promise<DeleteResponse> {
  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}/api/releases/${encodeURIComponent(code)}`, {
      method: 'DELETE',
      headers: { 'X-Release-Key': cfg.writeKey },
    });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new ReleasesApiError((body as { error?: string }).error ?? `releases.uprough.net returned ${response.status}`, {
      status: response.status,
    });
  }
  return body as DeleteResponse;
}

export async function publishToWeb(
  cfg: ReleasesClientConfig,
  code: string,
  archivePath: string,
  imagePath: string,
  embedImagePaths: string[],
): Promise<PublishToWebResult> {
  const form = new FormData();
  form.append('archive', await openAsBlob(archivePath), path.basename(archivePath));
  form.append('image', await openAsBlob(imagePath), path.basename(imagePath));
  // Matched positionally on the server to this release's stored image-type
  // media embeds, in the same order - see routes.ts's publish handler.
  for (const embedImagePath of embedImagePaths) {
    form.append('embedImage', await openAsBlob(embedImagePath), path.basename(embedImagePath));
  }

  let response: Response;
  try {
    response = await fetch(`${cfg.baseUrl}/api/releases/${encodeURIComponent(code)}/publish`, {
      method: 'POST',
      headers: { 'X-Release-Key': cfg.writeKey },
      body: form,
    });
  } catch (err) {
    throw new ReleasesApiError(`could not reach releases.uprough.net: ${(err as Error).message}`);
  }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) {
    const errorBody = body as { error?: string; reason?: string; wordpressPostUrl?: string };
    throw new ReleasesApiError(errorBody.error ?? `releases.uprough.net returned ${response.status}`, {
      status: response.status,
      ...(errorBody.reason ? { reason: errorBody.reason } : {}),
      ...(response.status === 409 && errorBody.wordpressPostUrl ? { wordpressPostUrl: errorBody.wordpressPostUrl } : {}),
    });
  }
  return body as PublishToWebResult;
}
