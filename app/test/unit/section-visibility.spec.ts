import { describe, it, expect } from 'vitest';
import { visibleSections } from '../../src/shared/section-visibility';

describe('visibleSections', () => {
  it.each([
    ['uprough', 'releases-doorserver', {
      xFields: false, boards: false, historyEntry: true, releaseNumberPreview: true, releaseNumberText: false,
      mediaEmbeds: true, creatorsAutocomplete: true, dizReleaseLine: true, dizPreviewOnly: false,
      publishWebsite: true, revoke: true, aminet: true,
    }],
    ['xinnovations', 'releases-doorserver', {
      xFields: true, boards: true, historyEntry: true, releaseNumberPreview: true, releaseNumberText: false,
      mediaEmbeds: true, creatorsAutocomplete: true, dizReleaseLine: true, dizPreviewOnly: false,
      publishWebsite: true, revoke: true, aminet: true,
    }],
    ['placeholder', 'none', {
      xFields: false, boards: false, historyEntry: false, releaseNumberPreview: false, releaseNumberText: true,
      mediaEmbeds: false, creatorsAutocomplete: false, dizReleaseLine: false, dizPreviewOnly: true,
      publishWebsite: false, revoke: false, aminet: true,
    }],
    ['uprough', 'none', {
      xFields: false, boards: false, historyEntry: false, releaseNumberPreview: false, releaseNumberText: true,
      mediaEmbeds: false, creatorsAutocomplete: false, dizReleaseLine: true, dizPreviewOnly: false,
      publishWebsite: false, revoke: false, aminet: true,
    }],
  ] as const)('%s renderer with backend %s', (renderer, backend, expected) => {
    expect(visibleSections({ renderer, backend })).toEqual(expected);
  });
});
