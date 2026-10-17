import { openAsBlob } from 'fs';
import * as path from 'path';

/**
 * The AmiExpress door repository at doors.uprough.net.
 *
 * A /X Innovations release is a door, so it belongs there as well as on the
 * site - the four that exist were put there by hand. Submitting is
 * deliberately the same thing any member does: the archive goes into the
 * queue and a curator approves it. No curation key is baked into the app,
 * because the builds members receive are unsigned zips anyone can unpack,
 * and a key inside one is a key everybody has.
 *
 * The server does all the validating (extension allowlist, magic bytes
 * against the extension, an 8 MB cap, and a sha256 check against both the
 * catalogue and the queue), so a resubmitted archive comes back 409 with a
 * readable reason rather than queueing twice. Its wording is passed straight
 * through - it is better than anything this end could invent.
 */
export const DOOR_REPO_SUBMISSIONS_URL = 'https://doors.uprough.net/api/door-repo/submissions';

export interface DoorSubmissionResult {
  ok: boolean;
  /** What the door server said, in its own words. */
  message: string;
  /** Present when it was accepted into the queue. */
  id?: number;
}

export async function submitDoor(
  archivePath: string,
  url: string = DOOR_REPO_SUBMISSIONS_URL,
): Promise<DoorSubmissionResult> {
  const form = new FormData();
  form.append('file', await openAsBlob(archivePath), path.basename(archivePath));

  let response: Response;
  try {
    response = await fetch(url, { method: 'POST', body: form });
  } catch (err) {
    return { ok: false, message: `could not reach the door repository: ${(err as Error).message}` };
  }

  const body = (await response.json().catch(() => ({}))) as { message?: string; error?: string; id?: number };
  if (!response.ok) {
    return {
      ok: false,
      message: body.error ?? `the door repository returned ${response.status}`,
    };
  }
  return {
    ok: true,
    message: body.message ?? 'submitted to the door repository',
    ...(typeof body.id === 'number' ? { id: body.id } : {}),
  };
}
