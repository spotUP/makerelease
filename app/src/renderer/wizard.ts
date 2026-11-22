/**
 * Splits the tall release form into five pages. Pages are hidden with a
 * class, never the `hidden` attribute, because nativeFieldErrors skips
 * fields inside [hidden] and validation must still see every page.
 * The current page lives in memory only.
 */
import { setRevealHandler } from './field-errors.js';

const STEPS = ['Release', 'NFO', 'People', 'Extras', 'Files and build', 'Publish'];
const PUBLISH = STEPS.length - 1;
const BUILD = PUBLISH - 1;

const byId = <T extends HTMLElement>(id: string): T => {
  const el = document.getElementById(id);
  if (!el) throw new Error(`Missing element #${id}`);
  return el as T;
};

export function initWizard(): void {
  const pages = Array.from(document.querySelectorAll<HTMLElement>('[data-wizard-page]'));
  const bar = byId<HTMLElement>('wizard-steps');
  let current = 0;

  // A page is available while any of its sections is shown (the Publish page
  // also needs a finished build, which is what un-hides its sections).
  const available = (i: number): boolean => pages[i].querySelector('section:not([hidden])') !== null;

  const buttons = STEPS.map((label, i) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = label;
    b.addEventListener('click', () => show(i));
    bar.append(b);
    return b;
  });

  const nextAvailable = (from: number, dir: 1 | -1): number => {
    for (let i = from + dir; i >= 0 && i < pages.length; i += dir) if (available(i)) return i;
    return -1;
  };

  const navs = pages.map((_, i) => {
    const nav = document.createElement('div');
    nav.className = 'wizard-nav';
    const back = document.createElement('button');
    back.type = 'button';
    back.textContent = 'Back';
    back.addEventListener('click', () => show(nextAvailable(i, -1)));
    const next = document.createElement('button');
    next.type = 'button';
    next.textContent = 'Next';
    next.addEventListener('click', () => show(nextAvailable(i, 1)));
    nav.append(back, next);
    pages[i].append(nav);
    return { back, next };
  });

  function render(): void {
    if (!available(current)) current = Math.max(0, nextAvailable(current, -1));
    pages.forEach((page, i) => page.classList.toggle('wizard-page-active', i === current));
    buttons.forEach((b, i) => {
      const ok = available(i);
      b.hidden = !ok && i !== PUBLISH;
      b.disabled = !ok;
      // Only Publish stays visible while locked, so it must say why.
      b.title = i === PUBLISH && !ok ? 'Unlocks after Build (not Preview build) on the Files and build page.' : '';
      b.classList.toggle('wizard-step-current', i === current);
      if (i === current) b.setAttribute('aria-current', 'step');
      else b.removeAttribute('aria-current');
    });
    navs.forEach(({ back, next }, i) => {
      back.hidden = nextAvailable(i, -1) < 0;
      next.hidden = nextAvailable(i, 1) < 0;
    });
  }

  function show(i: number): void {
    if (i < 0 || i >= pages.length || !available(i)) return;
    current = i;
    render();
    window.scrollTo({ top: 0 });
  }

  // Sections come and go through their `hidden` attribute (group visibility,
  // publish after a build). A newly shown Publish page is switched to.
  let publishWasAvailable = available(PUBLISH);
  // Only page sections count: render() itself toggles `hidden` on the step
  // and Back/Next buttons, and reacting to those would loop forever.
  new MutationObserver((records) => {
    if (!records.some((r) => r.target instanceof HTMLElement && r.target.matches('[data-wizard-page] section'))) return;
    const now = available(PUBLISH);
    if (now && !publishWasAvailable) {
      publishWasAvailable = now;
      show(PUBLISH);
      return;
    }
    publishWasAvailable = now;
    if (!now && current === PUBLISH) current = BUILD;
    render();
  }).observe(document.body, { subtree: true, attributes: true, attributeFilter: ['hidden'] });

  setRevealHandler((el) => {
    const i = pages.findIndex((page) => page.contains(el));
    if (i >= 0) show(i);
  });

  render();
  initFolds();
  initRevokeDialog();
  initMenus();
}

/** Header dropdowns close after a choice or a click elsewhere. */
function initMenus(): void {
  const menus = Array.from(document.querySelectorAll<HTMLDetailsElement>('details.menu'));
  document.addEventListener('click', (e) => {
    for (const m of menus) if (!m.contains(e.target as Node) || e.target instanceof HTMLButtonElement) m.open = false;
  });
}

function initRevokeDialog(): void {
  const dialog = byId<HTMLDialogElement>('revoke-dialog');
  const open = byId<HTMLButtonElement>('open-revoke-btn');
  open.addEventListener('click', () => dialog.showModal());
  byId('revoke-close-btn').addEventListener('click', () => dialog.close());
  // The group may have no revoke feature: its section hides, so the opener goes too.
  const section = byId('cleanup-section');
  const sync = (): void => {
    open.hidden = section.hidden;
  };
  new MutationObserver(sync).observe(section, { attributes: true, attributeFilter: ['hidden'] });
  sync();
}

interface Fold {
  details: HTMLDetailsElement;
  summary: HTMLElement;
  text: () => string;
  /** True when the fold holds a required value that is still empty. */
  needsInput?: () => boolean;
}

let folds: Fold[] = [];

function initFolds(): void {
  const input = (id: string): HTMLInputElement | HTMLTextAreaElement => byId<HTMLInputElement | HTMLTextAreaElement>(id);
  const count = (n: number, one: string, many: string): string => `${n} ${n === 1 ? one : many}`;
  const fold = (id: string, text: () => string, needsInput?: () => boolean): Fold => ({
    details: byId<HTMLDetailsElement>(id),
    summary: byId(`${id}-summary`),
    text,
    needsInput,
  });
  folds = [
    fold('fold-format', () => `Archive format: ${document.querySelector<HTMLInputElement>('input[name="format"]:checked')?.value ?? 'none'}`),
    fold('fold-members', () => `Members: ${byId('members-tbody').children.length} listed`),
    fold('fold-news', () => {
      const lines = input('news-text').value.split('\n').filter((l) => l.trim() !== '').length;
      return lines === 0 ? 'News: empty' : `News: ${count(lines, 'line', 'lines')}`;
    }),
    fold(
      'fold-aminet-uploader',
      () => `Aminet uploader: ${input('aminet-uploader-name').value.trim() || 'no name'}, ${input('aminet-architecture').value.trim() || 'no architecture'}`,
      () => input('aminet-uploader-name').value.trim() === '' || input('aminet-uploader-email').value.trim() === '',
    ),
  ];
  const refresh = (): void => refreshFolds();
  document.addEventListener('input', refresh);
  document.addEventListener('change', refresh);
  new MutationObserver(refresh).observe(byId('members-tbody'), { childList: true });
  refreshFolds();
  applyFoldDefaults();
}

/** Updates every fold's summary line from the current field values. */
export function refreshFolds(): void {
  for (const f of folds) f.summary.textContent = f.text();
}

/** Opens folds whose required values are still empty. Call after fields are filled programmatically. */
export function applyFoldDefaults(): void {
  refreshFolds();
  for (const f of folds) if (f.needsInput?.()) f.details.open = true;
}
