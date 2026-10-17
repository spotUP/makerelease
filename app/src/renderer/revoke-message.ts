import { DeleteReleaseResult } from '../shared/types.js';

/**
 * The single sentence shown after a revoke. Pure and separate from main.ts so
 * it can be tested: a revoke touches four things (catalog row, release
 * number, published archive, WordPress post) and each of them can end up in a
 * different state, which is exactly the kind of message that goes silently
 * wrong when it lives inline in an event handler.
 */
export function describeRevoke(result: DeleteReleaseResult): string {
  if (result.wasDraft) {
    return (
      `Revoked release ${result.releaseNumber}. It was never published, so nothing was ever ` +
      `public — release number ${result.releaseNumber} is free again.`
    );
  }

  const parts = [`Revoked release ${result.releaseNumber}.`];
  if (result.wordpressPostTrashed) {
    parts.push('Its WordPress post is in the trash (recoverable in wp-admin for 30 days).');
  } else if (result.wordpressPostUrl !== null) {
    parts.push(
      `Its WordPress post could NOT be trashed (${result.wordpressPostError ?? 'unknown error'}) — ` +
        `remove it by hand: ${result.wordpressPostUrl}`,
    );
  }
  if (result.wasPublished && !result.archiveRemoved) {
    parts.push('Its published archive could not be removed — check the server logs.');
  }
  return parts.join(' ');
}
