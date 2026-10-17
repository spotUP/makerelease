import * as fs from 'fs';
import * as path from 'path';
import { DIVISIONS } from '../../src/shared/divisions';
import { ReleaseFormData, ReleaseHistoryEntry } from '../../src/shared/types';
import { generateOutputs } from '../../src/main/nfo-generator';

/**
 * One fixed Up Rough release, frozen from the renderer as it stood before the
 * group-profile refactor. The golden files in test/fixtures were written from
 * this input by writeFixtures() once; from then on ur-golden.spec.ts proves
 * the Up Rough NFO and DIZ never move.
 */

const bannerRows = DIVISIONS.uprough.defaultDizBanner.split('\n');
bannerRows[8] = '|   HYPOCRISY - A500 HDD DEMO FROM CS 2025 |';

export const GOLD_RELEASE_NUMBER = 218;

export const GOLD_FORM: ReleaseFormData = {
  profileId: 'uprough',
  division: 'uprough',
  code: 'gold1',
  format: 'LHA',
  releaseName: 'Hypocrisy',
  platformTag: 'A500',
  releaseNotesText: 'A demo for the Amiga 500 with a hard drive.\n\nReleased at Computer Space 2025.',
  newsText: 'Two new members joined this year.',
  dizBannerText: bannerRows.join('\n'),
  members: [
    { nickname: 'Spot', role: 'code' },
    { nickname: 'Hoffman', role: 'music' },
  ],
  entryTitle: 'Hypocrisy',
  entryCategory: 'demo',
  entryCreators: 'Spot, Hoffman',
  selectedPaths: [],
  description: 'A500 HDD demo',
  credits: [
    { role: 'code', name: 'Spot' },
    { role: 'music', name: 'Hoffman' },
    { role: 'graphics', name: 'Lazy' },
  ],
  mediaEmbeds: [],
  releaseDate: '2025-10-18',
};

export const GOLD_HISTORY: ReleaseHistoryEntry[] = [
  { releaseNumber: '218', filenames: 'up-gold1.lha', title: 'Hypocrisy', category: 'demo', creators: 'Spot, Hoffman' },
  {
    releaseNumber: '217',
    filenames: 'up-evoke.lha',
    title: 'Evoke invitation',
    category: 'intro',
    creators:
      'Spot, Hoffman, Lazy, Zalo, Rasmus, Mixer, Juice, Pinchy, Dr Doom, Antichrist and everyone else at the party place',
  },
  { releaseNumber: '216', filenames: 'up-md15.lha', title: 'More Doors Cursor Menu', category: 'tool', creators: 'Zalo' },
];

export const GOLD_NFO_PATH = path.join(__dirname, '../fixtures/UP-GOLD.NFO');
export const GOLD_DIZ_PATH = path.join(__dirname, '../fixtures/UP-GOLD.DIZ');

export const GOLD_PROFILE = { filePrefix: 'UP-', label: 'Up Rough', url: 'https://uprough.net', renderer: 'uprough' } as const;

export function renderGold(): { nfoText: string; dizText: string } {
  const out = generateOutputs(GOLD_PROFILE, GOLD_HISTORY, GOLD_RELEASE_NUMBER, GOLD_FORM);
  if (out.nfoText === null) throw new Error('the Up Rough renderer always writes an NFO');
  return { nfoText: out.nfoText, dizText: out.dizText };
}

export function writeFixtures(): void {
  const { nfoText, dizText } = renderGold();
  fs.writeFileSync(GOLD_NFO_PATH, nfoText, 'latin1');
  fs.writeFileSync(GOLD_DIZ_PATH, dizText, 'latin1');
}
