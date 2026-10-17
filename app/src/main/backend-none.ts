import type { GroupProfile } from '../shared/profile';
import type { ReleaseBackend } from './backend';

export const NO_SERVER_MESSAGE = 'This group has no release server';

/**
 * A group without a release server: nothing is numbered, nothing is
 * remembered between releases, and nothing leaves the machine. The profile's
 * own member list is the whole seed.
 */
export function noneBackend(profile: Pick<GroupProfile, 'members'>): ReleaseBackend {
  const nothingNumbered = async () => ({ releaseNumber: 0, history: [] });
  return {
    seed: async () => ({
      members: profile.members,
      boards: [],
      nextReleaseNumber: 0,
      history: [],
      currentDizText: '',
      revision: 0,
    }),
    // No catalogue to collide with; buildRelease's local pre-flight still
    // refuses to overwrite a leftover.
    codeExists: async () => false,
    create: nothingNumbered,
    preview: nothingNumbered,
    releaseList: async () => [],
    publishToWeb: async () => {
      throw new Error(NO_SERVER_MESSAGE);
    },
    deleteRelease: async () => {
      throw new Error(NO_SERVER_MESSAGE);
    },
    afterBuild: async () => undefined,
  };
}
