import { describe, it, expect } from 'vitest';
import { noneBackend, NO_SERVER_MESSAGE } from '../../src/main/backend-none';
import type { ReleaseFormData } from '../../src/shared/types';

describe('noneBackend', () => {
  const members = [{ nickname: 'coder', role: 'code' }];
  const backend = noneBackend({ members });
  const form = { code: 'DEMO1' } as ReleaseFormData;

  it('seeds from the profile members alone, with nothing numbered', async () => {
    expect(await backend.seed()).toEqual({
      members,
      boards: [],
      nextReleaseNumber: 0,
      history: [],
      currentDizText: '',
      revision: 0,
    });
  });

  it('never reports a code as taken; there is no catalogue to collide with', async () => {
    expect(await backend.codeExists('DEMO1')).toBe(false);
  });

  it('numbers neither a real build nor a dry run', async () => {
    expect(await backend.create(form)).toEqual({ releaseNumber: 0, history: [] });
    expect(await backend.preview(form)).toEqual({ releaseNumber: 0, history: [] });
  });

  it('has no release list and nothing to do after a build', async () => {
    expect(await backend.releaseList(form, 0)).toEqual([]);
    expect(await backend.afterBuild('/out/EX-DEMO1.LHA')).toBeUndefined();
  });

  it('refuses to publish or revoke, naming the missing server', async () => {
    await expect(backend.publishToWeb('DEMO1', '/a', '/i', [])).rejects.toThrow(NO_SERVER_MESSAGE);
    await expect(backend.deleteRelease('DEMO1')).rejects.toThrow(NO_SERVER_MESSAGE);
  });
});
