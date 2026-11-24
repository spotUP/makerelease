// Guard for a private build: app/private-profiles/ (git-ignored) must hold the
// maintainer groups, keys included, or a "private" build would silently be a
// public-equivalent one. Ported from the Electron dist-private.mjs guard.
// Usage: node check-private-profiles.mjs [dir]  (default app/private-profiles)
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const source = process.argv[2] ?? path.join(repo, 'app', 'private-profiles');

function fail(message) {
  console.error(`check-private-profiles: ${message}`);
  process.exit(1);
}

if (!existsSync(source) || !statSync(source).isDirectory()) {
  fail(`${source} is missing. Copy the group folders there first (see app/README.md).`);
}
const ids = readdirSync(source, { withFileTypes: true })
  .filter((entry) => entry.isDirectory() && !entry.name.startsWith('.'))
  .map((entry) => entry.name)
  .sort();
if (ids.length === 0) fail(`${source} holds no groups.`);
if (ids.includes('example')) fail(`${source} must not contain the example group.`);
for (const id of ids) {
  const file = path.join(source, id, 'profile.json');
  if (!existsSync(file)) fail(`${id}/ has no profile.json.`);
  let raw;
  try {
    raw = JSON.parse(readFileSync(file, 'utf8').replace(/^﻿/, ''));
  } catch (err) {
    fail(`${id}/profile.json is not valid JSON (${err.message}).`);
  }
  for (const field of ['label', 'url', 'renderer', 'backend']) {
    if (typeof raw[field] !== 'string' || raw[field].trim() === '') {
      fail(`${id}/profile.json: "${field}" is missing or empty.`);
    }
  }
  if (raw.backend === 'releases-doorserver' && (typeof raw.server?.writeKey !== 'string' || raw.server.writeKey === '')) {
    fail(`${id}/profile.json: a private build needs a real server.writeKey, or it cannot publish.`);
  }
}
console.log(`check-private-profiles: OK (${ids.join(', ')})`);
