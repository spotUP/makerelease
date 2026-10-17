import type { ProfileSummary } from './profile';

/**
 * Which parts of the release form a group sees. A section that only feeds
 * the release server (history entry, media embeds, website publish, revoke)
 * means nothing to a group without one, and the /X board list means nothing
 * outside /X. Kept pure so the whole table is tested without a DOM.
 */
export interface SectionVisibility {
  xFields: boolean;
  boards: boolean;
  historyEntry: boolean;
  releaseNumberPreview: boolean;
  releaseNumberText: boolean;
  mediaEmbeds: boolean;
  creatorsAutocomplete: boolean;
  dizReleaseLine: boolean;
  dizPreviewOnly: boolean;
  publishWebsite: boolean;
  revoke: boolean;
  aminet: boolean;
}

export function visibleSections(p: Pick<ProfileSummary, 'renderer' | 'backend'>): SectionVisibility {
  const server = p.backend === 'releases-doorserver';
  return {
    xFields: p.renderer === 'xinnovations',
    boards: p.renderer === 'xinnovations',
    historyEntry: server,
    releaseNumberPreview: server,
    releaseNumberText: !server,
    mediaEmbeds: server,
    creatorsAutocomplete: server,
    dizReleaseLine: p.renderer !== 'placeholder',
    dizPreviewOnly: p.renderer === 'placeholder',
    publishWebsite: server,
    revoke: server,
    aminet: true,
  };
}
