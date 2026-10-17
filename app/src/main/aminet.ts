import * as path from 'path';
import type { AminetUploadOutcome, AminetUploadRequest } from '../shared/types';
import { aminetFileNameProblems, aminetFileNames, aminetReadmeProblems } from '../shared/aminet-readme';
import { toAsciiSafe, writeLatin1 } from './encoding';
import { runExternal } from './external-tool';

/**
 * Aminet's incoming directory (wiki.aminet.net/Uploading_instructions):
 * anonymous FTP, the uploader's email as the password, and a .readme next to
 * every file. The file lands in the public archive once a moderator has
 * looked at it, and from then on it cannot be unpublished from here - which
 * is why this runs at Publish time, after the website publish succeeded, and
 * never at Build time.
 */
export const AMINET_UPLOAD_URL = 'ftp://main.aminet.net/new/';

const CURL_INSTALL_HINTS: Partial<Record<NodeJS.Platform, string>> = {
  darwin: 'it ships with macOS; reinstall the command line tools or "brew install curl"',
  linux: 'install it with your package manager, e.g. "sudo apt install curl"',
  win32: 'it ships with Windows 10 and later; otherwise install it from curl.se',
};

export interface AminetUploadDeps {
  run: typeof runExternal;
  writeReadme: typeof writeLatin1;
}

const defaultDeps: AminetUploadDeps = { run: runExternal, writeReadme: writeLatin1 };

/**
 * Normalises the readme to what Aminet accepts: LF line ends, printable
 * ASCII (Aminet's index is read on machines with every charset under the
 * sun, so accented letters are folded like the NFO does), one trailing
 * newline.
 */
export function normalizeReadmeText(text: string): string {
  const lf = toAsciiSafe(text.replace(/\r\n?/g, '\n'));
  return lf.endsWith('\n') ? lf : `${lf}\n`;
}

/**
 * Uploads the archive and its readme to Aminet's incoming directory. Never
 * throws: a failed Aminet upload is something to report next to the
 * website's publish result, not something to make the publish look failed.
 * The readme is written next to the archive and kept, so a failed upload
 * can be redone by hand with the same two files.
 */
export async function uploadToAminet(
  request: AminetUploadRequest,
  archivePath: string,
  deps: AminetUploadDeps = defaultDeps,
): Promise<AminetUploadOutcome> {
  const names = aminetFileNames(path.basename(archivePath));
  const readmeText = normalizeReadmeText(request.readmeText);
  const problems = [...aminetFileNameProblems(names), ...aminetReadmeProblems(readmeText)];
  if (problems.length) {
    return { ok: false, message: `not uploaded to Aminet: ${problems.join('; ')}` };
  }
  const email = request.uploaderEmail.trim();
  if (!email.includes('@')) {
    return { ok: false, message: 'not uploaded to Aminet: the uploader email is missing' };
  }

  const readmePath = path.join(path.dirname(archivePath), names.readme);
  try {
    await deps.writeReadme(readmePath, readmeText);
  } catch (err) {
    return { ok: false, message: `not uploaded to Aminet: could not write ${readmePath}: ${(err as Error).message}` };
  }

  // Archive first, readme second: Aminet's intake pairs a readme with the
  // file of the same base name, and a readme without its file is the case
  // the uploading instructions warn about.
  const uploads: Array<[string, string]> = [
    [archivePath, names.archive],
    [readmePath, names.readme],
  ];
  for (const [localPath, remoteName] of uploads) {
    try {
      await deps.run(
        'curl',
        [
          '--silent',
          '--show-error',
          '--fail',
          '--ftp-pasv',
          '--user',
          `anonymous:${email}`,
          '--upload-file',
          localPath,
          `${AMINET_UPLOAD_URL}${remoteName}`,
        ],
        { installHint: CURL_INSTALL_HINTS[process.platform] },
      );
    } catch (err) {
      return {
        ok: false,
        message: `Aminet upload of ${remoteName} failed: ${(err as Error).message}`,
        readmePath,
      };
    }
  }

  return {
    ok: true,
    message: `uploaded ${names.archive} and ${names.readme} to Aminet (it appears once a moderator has looked at it)`,
    readmePath,
  };
}
