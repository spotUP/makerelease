import { AMINET_TYPES } from './aminet-types.js';

// Targets verified against AMINET_TYPES. Musicdisk -> mods/sets (module collections; demo/disk is "demos on diskettes").
// Video -> pix/anim (Aminet has no gfx/anim; animations live in pix/). ASCII has no real directory -> ''.
const BY_CATEGORY: Record<string, string> = {
  intro: 'demo/intro',
  musicdisk: 'mods/sets',
  music: 'mods/misc',
  graphics: 'pix/art',
  tool: 'util/misc',
  door: 'comm/bbs',
  video: 'pix/anim',
};

/** Best-guess Aminet directory for a release category and platform tag, or '' when none fits. */
export function guessAminetType(category: string, platformTag: string): string {
  const cat = category.trim().toLowerCase();
  let guess = BY_CATEGORY[cat] ?? '';
  if (cat === 'demo') {
    const tag = platformTag.trim().toUpperCase();
    guess = tag === 'A1200' ? 'demo/aga' : tag === 'A500' ? 'demo/ecs' : 'demo/misc';
  }
  return AMINET_TYPES.some((t) => t.type === guess) ? guess : '';
}
