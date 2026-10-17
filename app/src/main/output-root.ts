import * as path from 'path';

/** The Electron locations we are willing to put the output folder under, most preferred first. */
export type OutputRootLocation = 'documents' | 'home' | 'userData';

export type PathLookup = (name: OutputRootLocation) => string;

const FALLBACK_ORDER: readonly OutputRootLocation[] = ['documents', 'home', 'userData'];

/**
 * Where a PACKAGED build writes the archive and the unpacked release folder.
 * `folderName` is per profile ("<label> Releases"), so each group's builds
 * land in their own folder.
 *
 * Electron's app.getPath() throws when a location cannot be resolved rather
 * than returning empty — 'documents' in particular depends on XDG user dirs on
 * Linux, which a minimal install (or one with HOME unset) may not provide.
 * This runs during startup, so an unhandled throw would kill the app before
 * any window or error dialog could exist. Walk towards progressively more
 * certain locations instead of trusting the first one.
 */
export function resolvePackagedOutputRoot(getPath: PathLookup, folderName: string): string {
  for (const location of FALLBACK_ORDER) {
    try {
      const base = getPath(location);
      if (base) return path.join(base, folderName);
    } catch {
      // Unresolvable on this system — try the next, more reliable, location.
    }
  }
  throw new Error(
    `Could not resolve any of ${FALLBACK_ORDER.join(', ')} to put the "${folderName}" folder in`,
  );
}
