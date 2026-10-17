import { MediaEmbedFormEntry } from '../shared/types';

// Each row is either a YouTube embed (caption + URL) or an image embed
// (caption + a locally-picked file, uploaded only at Publish time - see
// main.ts's lastEmbedImagePaths). Row order is meaningful: it's the order
// embeds render in the post, and getEmbedImagePaths()'s order must match
// getMediaEmbeds()'s image-type entries exactly, since the server matches
// uploaded embedImage files to stored embeds positionally.
export class MediaEmbedsTable {
  private container: HTMLElement;
  private pickImage: () => Promise<string | null>;

  constructor(container: HTMLElement, pickImage: () => Promise<string | null>) {
    this.container = container;
    this.pickImage = pickImage;
  }

  addRow(): void {
    const row = document.createElement('div');
    row.className = 'media-embed-row';

    const typeSelect = document.createElement('select');
    for (const [value, label] of [
      ['youtube', 'YouTube'],
      ['image', 'Image'],
    ]) {
      const option = document.createElement('option');
      option.value = value;
      option.textContent = label;
      typeSelect.appendChild(option);
    }

    const captionInput = document.createElement('input');
    captionInput.type = 'text';
    captionInput.placeholder = 'Caption (optional)';

    const urlInput = document.createElement('input');
    urlInput.type = 'text';
    urlInput.placeholder = 'https://youtu.be/...';

    const pickImageBtn = document.createElement('button');
    pickImageBtn.type = 'button';
    pickImageBtn.textContent = 'Choose image...';
    pickImageBtn.hidden = true;

    const imagePathSpan = document.createElement('span');
    imagePathSpan.className = 'status';
    imagePathSpan.hidden = true;
    let imagePath: string | null = null;

    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.textContent = 'Remove';
    removeBtn.className = 'danger';
    removeBtn.addEventListener('click', () => row.remove());

    const applyTypeVisibility = (): void => {
      const isYoutube = typeSelect.value === 'youtube';
      urlInput.hidden = !isYoutube;
      pickImageBtn.hidden = isYoutube;
      imagePathSpan.hidden = isYoutube;
    };
    typeSelect.addEventListener('change', applyTypeVisibility);
    applyTypeVisibility();

    pickImageBtn.addEventListener('click', async () => {
      const picked = await this.pickImage();
      if (picked) {
        imagePath = picked;
        imagePathSpan.textContent = picked;
        imagePathSpan.hidden = false;
      }
    });

    // Read back at getEmbedImagePaths()/getMediaEmbeds() time via a closure
    // property on the row element itself, rather than a parallel array kept
    // in sync by index - rows can be removed independently via their own
    // Remove button, which would desync any index-based side table.
    (row as HTMLDivElement & { getImagePath: () => string | null }).getImagePath = () => imagePath;

    row.append(typeSelect, captionInput, urlInput, pickImageBtn, imagePathSpan, removeBtn);
    this.container.appendChild(row);
  }

  private rows(): HTMLDivElement[] {
    return Array.from(this.container.querySelectorAll<HTMLDivElement>('.media-embed-row'));
  }

  /** Non-empty only if every 'image' row has a picked local file - blocks Build with a clear message otherwise. */
  getValidationError(): string | null {
    for (const row of this.rows()) {
      const typeSelect = row.querySelector('select') as HTMLSelectElement;
      if (typeSelect.value !== 'image') continue;
      const getImagePath = (row as HTMLDivElement & { getImagePath: () => string | null }).getImagePath;
      if (!getImagePath()) {
        return 'Every image media embed needs a picked image file — choose one or remove the row.';
      }
    }
    return null;
  }

  getMediaEmbeds(): MediaEmbedFormEntry[] {
    const result: MediaEmbedFormEntry[] = [];
    for (const row of this.rows()) {
      const inputs = row.querySelectorAll('input');
      const typeSelect = row.querySelector('select') as HTMLSelectElement;
      const caption = (inputs[0] as HTMLInputElement).value.trim();
      if (typeSelect.value === 'youtube') {
        const youtubeUrl = (inputs[1] as HTMLInputElement).value.trim();
        if (!youtubeUrl) continue; // skip an empty row rather than sending an invalid embed
        result.push({ type: 'youtube', caption, youtubeUrl });
      } else {
        result.push({ type: 'image', caption });
      }
    }
    return result;
  }

  /** Ordered local paths for 'image' rows only, matching getMediaEmbeds()'s image-type entry order exactly. */
  getEmbedImagePaths(): string[] {
    const paths: string[] = [];
    for (const row of this.rows()) {
      const typeSelect = row.querySelector('select') as HTMLSelectElement;
      if (typeSelect.value !== 'image') continue;
      const getImagePath = (row as HTMLDivElement & { getImagePath: () => string | null }).getImagePath;
      const p = getImagePath();
      if (p) paths.push(p);
    }
    return paths;
  }
}
