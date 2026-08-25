// src/main/config.ts
//
// RELEASES_WRITE_KEY resolution order:
//   1. INJECTED_RELEASES_WRITE_KEY, baked into config.generated.ts by
//      scripts/inject-release-key.js during `npm run dist` (see
//      "Distributing a real build" in README.md). This is what a packaged
//      .app launched from Finder actually uses — Finder-launched apps
//      inherit no shell environment, so process.env alone can never carry a
//      real key into a distributed build.
//   2. process.env.RELEASES_WRITE_KEY, a dev-time override for running
//      against a local/staging releases-doorserver without rebuilding.
//   3. PLACEHOLDER_KEY, which the server always rejects with 401. When this
//      is what's active, loadSeedState's IPC handler flags it in SeedState
//      so the renderer can show a visible warning instead of only logging
//      to main-process stdout (which a Finder-launched user never sees).
import { INJECTED_RELEASES_WRITE_KEY } from './config.generated';

// releases.uprough.net now has its own DNS record and nginx vhost (see
// /etc/nginx/sites-available/releases.uprough.net on the server) proxying to
// the same releases-doorserver process that www.uprough.net/api/releases
// still reaches. Both origins work and must keep working: builds already
// distributed to members are pinned to the www one, so the path-mounted
// block in the uprough.net vhost cannot be removed.
// releases-client.ts appends /api/releases/... itself, so this is the bare
// origin.
const DEFAULT_BASE_URL = 'https://releases.uprough.net';
const PLACEHOLDER_KEY = 'REPLACE_ME_BEFORE_DISTRIBUTING';

export interface ReleasesClientConfig {
  baseUrl: string;
  writeKey: string;
  isPlaceholderKey: boolean;
}

export function loadReleasesClientConfig(env: NodeJS.ProcessEnv = process.env): ReleasesClientConfig {
  const writeKey = INJECTED_RELEASES_WRITE_KEY ?? env.RELEASES_WRITE_KEY ?? PLACEHOLDER_KEY;
  const isPlaceholderKey = writeKey === PLACEHOLDER_KEY;
  if (isPlaceholderKey) {
    console.warn(
      '[WARN] RELEASES_WRITE_KEY is not set — using a placeholder that the server will reject. ' +
        'Set RELEASES_WRITE_KEY before distributing this build.',
    );
  }
  return {
    baseUrl: env.RELEASES_API_BASE_URL ?? DEFAULT_BASE_URL,
    writeKey,
    isPlaceholderKey,
  };
}
