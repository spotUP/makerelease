import { describe, expect, it } from 'vitest';
import { describeRevoke } from '../../src/renderer/revoke-message';
import { DeleteReleaseResult } from '../../src/shared/types';

const base: DeleteReleaseResult = {
  code: 'TEST0',
  releaseNumber: 218,
  wasPublished: false,
  wasDraft: true,
  wordpressPostUrl: null,
  archiveRemoved: false,
  wordpressPostTrashed: false,
  wordpressPostError: null,
};

describe('describeRevoke', () => {
  it('says an abandoned build was never public and its number is free again', () => {
    const message = describeRevoke(base);
    expect(message).toContain('never published');
    expect(message).toContain('218 is free again');
  });

  it('reports the WordPress post as trashed and recoverable for a published release', () => {
    const message = describeRevoke({
      ...base,
      wasDraft: false,
      wasPublished: true,
      archiveRemoved: true,
      wordpressPostUrl: 'https://www.uprough.net/218-thing/',
      wordpressPostTrashed: true,
    });
    expect(message).toContain('trash');
    expect(message).toContain('30 days');
  });

  it('hands back the post URL and the reason when the post could not be trashed', () => {
    const message = describeRevoke({
      ...base,
      wasDraft: false,
      wasPublished: true,
      archiveRemoved: true,
      wordpressPostUrl: 'https://www.uprough.net/218-thing/',
      wordpressPostTrashed: false,
      wordpressPostError: 'WordPress /wp-json/wp/v2/posts/9 returned 403: not allowed',
    });
    expect(message).toContain('could NOT be trashed');
    expect(message).toContain('403');
    expect(message).toContain('https://www.uprough.net/218-thing/');
  });

  it('flags an archive the server could not remove', () => {
    const message = describeRevoke({
      ...base,
      wasDraft: false,
      wasPublished: true,
      archiveRemoved: false,
      wordpressPostUrl: 'https://www.uprough.net/218-thing/',
      wordpressPostTrashed: true,
    });
    expect(message).toContain('archive could not be removed');
  });
});
