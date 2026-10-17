import * as path from 'path';
import type { ReleaseFormData } from '../../src/shared/types';

/** The example profile as profile.json defines it. */
export const EX_PROFILE = { filePrefix: 'EX-', label: 'Example Group', url: 'https://example.net' } as const;

export const EX_FORM: ReleaseFormData = {
  profileId: 'example',
  code: 'test1',
  format: 'LHA',
  releaseName: 'Demo Disk',
  platformTag: 'A500',
  releaseDate: '2026-10-17',
  releaseNumberText: '007',
  description: 'A demo disk released at the party.',
  releaseNotesText: '\n\nFirst line of notes.   \r\nSecond line, $& kept.\n\n',
  newsText: '',
  dizBannerText: '',
  members: [
    { nickname: 'coder', role: 'code' },
    { nickname: 'pixelartist', role: 'graphics' },
  ],
  entryTitle: '',
  entryCategory: '',
  entryCreators: 'coder and pixelartist',
  selectedPaths: [],
  credits: [
    { role: 'Code', name: 'coder' },
    { role: ' ', name: '' },
    { role: 'Graphics', name: 'pixelartist' },
  ],
  mediaEmbeds: [],
};

export const EX_NFO_PATH = path.join(__dirname, '../fixtures/EX-TEST1.NFO');
export const EX_DIZ_PATH = path.join(__dirname, '../fixtures/EX-TEST1.DIZ');
