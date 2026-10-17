import { describe, expect, it } from 'vitest';
import { BuiltRelease, builtReleaseFor } from '../../src/renderer/built-release';

const built: BuiltRelease = {
  profileId: 'uprough',
  code: 'TEST0',
  archivePath: '/out/UP-TEST0.lha',
  embedImagePaths: [],
};

describe('builtReleaseFor', () => {
  it('offers the build for publishing while its own profile is selected', () => {
    expect(builtReleaseFor(built, 'uprough')).toBe(built);
  });

  it('refuses to publish a build after switching to another profile', () => {
    expect(builtReleaseFor(built, 'xinnovations')).toBeNull();
  });

  it('has nothing to publish before any build', () => {
    expect(builtReleaseFor(null, 'uprough')).toBeNull();
  });
});
