// Private distribution: the same app, but with the maintainer's groups from
// app/private-profiles/ (key included) bundled for first-launch seeding.
// The source folder is git-ignored and must already hold the groups; this
// script refuses to build a "private" dist without them, so a misplaced
// folder can never silently produce a public-equivalent build.
import { build } from 'electron-builder';
import { Platform } from 'app-builder-lib';
import { Arch } from 'builder-util';
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const appDir = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const source = path.join(appDir, 'private-profiles');

function fail(message) {
  console.error(`dist-private: ${message}`);
  process.exit(1);
}

// Guardrails: a private build without our groups, or with a keyless one that
// could not publish, would be worse than no build at all.
if (!existsSync(source) || !statSync(source).isDirectory()) {
  fail('app/private-profiles/ is missing. Copy the group folders there first (see app/README.md).');
}
const ids = readdirSync(source, { withFileTypes: true })
  .filter((entry) => entry.isDirectory() && !entry.name.startsWith('.'))
  .map((entry) => entry.name)
  .sort();
if (ids.length === 0) fail('app/private-profiles/ holds no groups.');
if (ids.includes('example')) fail('app/private-profiles/ must not contain the example group.');
for (const id of ids) {
  const file = path.join(source, id, 'profile.json');
  if (!existsSync(file)) fail(`${id}/ has no profile.json.`);
  let raw;
  try {
    raw = JSON.parse(readFileSync(file, 'utf8').replace(/^\uFEFF/, ''));
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

const pkg = JSON.parse(readFileSync(path.join(appDir, 'package.json'), 'utf8'));
const targets = new Map([
  ...Platform.MAC.createTarget(['dmg', 'zip'], Arch.x64, Arch.arm64),
  ...Platform.WINDOWS.createTarget('portable', Arch.x64),
  ...Platform.LINUX.createTarget(['AppImage', 'deb'], Arch.x64),
]);

const artifacts = await build({
  projectDir: appDir,
  config: {
    ...pkg.build,
    artifactName: '${productName}-${version}-private-${arch}.${ext}',
    // extraResources (not extraFiles): the one location that is
    // <resources>/ on every platform (Contents/Resources on mac), matching
    // the process.resourcesPath lookup at startup.
    extraResources: [{ from: 'private-profiles', to: 'private-profiles' }],
  },
  targets,
});
console.log(artifacts.join('\n'));
