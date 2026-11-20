// The renderer is a file:// page (CSP img-src allows file:), so a picked
// path is shown directly - no IPC, no file bytes pass through the renderer.
const THUMB_EXTENSIONS = /\.(png|jpe?g|gif|webp)$/i;

function fileName(path: string): string {
  return path.split(/[\\/]/).pop() ?? path;
}

/** A small <img> for a picked image path, or null when Chromium cannot show the type. */
export function createThumb(path: string): HTMLImageElement | null {
  if (!THUMB_EXTENSIONS.test(path)) return null;
  const img = document.createElement('img');
  img.className = 'thumb';
  img.alt = fileName(path);
  const normalized = path.replace(/\\/g, '/');
  img.src = encodeURI(`file://${normalized.startsWith('/') ? '' : '/'}${normalized}`).replace(/#/g, '%23').replace(/\?/g, '%3F');
  return img;
}
