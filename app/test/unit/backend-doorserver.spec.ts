import { describe, it, expect } from 'vitest';
import { doorserverBackend } from '../../src/main/backend-doorserver';
import { backendFor } from '../../src/main/backend';
import { parseProfile, type GroupProfile } from '../../src/shared/profile';

// The server behaviour itself (stale drafts, numbering warning, door
// submission, the /X release list) is driven through the IPC handlers in
// ipc-handlers.spec.ts, which proves it is wired; this file covers what only
// the backend seam decides.
describe('doorserverBackend', () => {
  const placeholder: GroupProfile = {
    id: 'odd', label: 'Odd', url: 'https://odd.net', filePrefix: '', codeLength: 5,
    renderer: 'placeholder', backend: 'releases-doorserver',
    server: { baseUrl: 'https://x', writeKey: 'k' }, members: [],
  };

  it('refuses a placeholder profile that bypassed parseProfile', () => {
    expect(() => doorserverBackend(placeholder, {})).toThrow('odd: the release server needs renderer');
  });

  it('backendFor picks the server only for a releases-doorserver profile', async () => {
    const none = parseProfile(
      { label: 'Ex', url: 'https://e', renderer: 'placeholder', members: [{ nickname: 'a', role: 'b' }] },
      'ex', 'ex/profile.json',
    );
    expect((await backendFor(none, {}).seed()).members).toEqual([{ nickname: 'a', role: 'b' }]);
    const server = parseProfile(
      { label: 'UR', url: 'https://u', renderer: 'uprough', backend: 'releases-doorserver', server: { baseUrl: 'https://x', writeKey: 'k' } },
      'ur', 'ur/profile.json',
    );
    expect(backendFor(server, {}).afterBuild).toBeTypeOf('function');
    await expect(backendFor(none, {}).publishToWeb('A', 'b', 'c', [])).rejects.toThrow('no release server');
  });
});
