/**
 * Short notices in the corner for outcomes the user should see wherever
 * they have scrolled to: a build, publish or upload finishing or failing.
 * The status line next to each button keeps the full text; the toast only
 * says that something happened and how it went.
 */
export type ToastKind = 'ok' | 'error' | 'info';

const DURATION_MS: Record<ToastKind, number> = { ok: 5000, info: 5000, error: 10000 };

function container(): HTMLElement {
  let region = document.getElementById('toasts');
  if (!region) {
    region = document.createElement('div');
    region.id = 'toasts';
    region.setAttribute('aria-live', 'polite');
    document.body.append(region);
  }
  return region;
}

export function toast(kind: ToastKind, message: string): void {
  const item = document.createElement('div');
  item.className = `toast ${kind}`;
  item.setAttribute('role', kind === 'error' ? 'alert' : 'status');
  const text = document.createElement('span');
  text.textContent = message;
  const close = document.createElement('button');
  close.type = 'button';
  close.className = 'toast-close';
  close.setAttribute('aria-label', 'Dismiss');
  close.textContent = 'Close';
  const remove = (): void => item.remove();
  close.addEventListener('click', remove);
  item.append(text, close);
  container().append(item);
  setTimeout(remove, DURATION_MS[kind]);
}
