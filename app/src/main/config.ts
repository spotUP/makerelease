// src/main/config.ts
const DEFAULT_BASE_URL = 'https://releases.uprough.net';
const PLACEHOLDER_KEY = 'REPLACE_ME_BEFORE_DISTRIBUTING';

export interface ReleasesClientConfig {
  baseUrl: string;
  writeKey: string;
}

export function loadReleasesClientConfig(env: NodeJS.ProcessEnv = process.env): ReleasesClientConfig {
  const writeKey = env.RELEASES_WRITE_KEY ?? PLACEHOLDER_KEY;
  if (writeKey === PLACEHOLDER_KEY) {
    console.warn(
      '[WARN] RELEASES_WRITE_KEY is not set — using a placeholder that the server will reject. ' +
        'Set RELEASES_WRITE_KEY before distributing this build.',
    );
  }
  return {
    baseUrl: env.RELEASES_API_BASE_URL ?? DEFAULT_BASE_URL,
    writeKey,
  };
}
