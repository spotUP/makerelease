import { ReleaseFormat, ReleaseFormData } from '../shared/types';
import { MemberlistTable } from './memberlist-table.js';
import { attachOverwriteGrid, toGridText } from './diz-grid.js';
import { attachCreatorsAutocomplete } from './creators-autocomplete.js';

const $ = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;

const codeInput = $<HTMLInputElement>('code');
const codeStatus = $<HTMLDivElement>('code-status');
const releaseNameInput = $<HTMLInputElement>('release-name');
const platformTagInput = $<HTMLInputElement>('platform-tag');
const releaseNotesTextArea = $<HTMLTextAreaElement>('release-notes-text');
const newsTextArea = $<HTMLTextAreaElement>('news-text');
const membersTbody = $<HTMLTableSectionElement>('members-tbody');
const addMemberBtn = $<HTMLButtonElement>('add-member-btn');
const releaseNumberPreview = $<HTMLDivElement>('release-number-preview');
const entryTitleInput = $<HTMLInputElement>('entry-title');
const entryCategoryInput = $<HTMLSelectElement>('entry-category');
const entryCreatorsInput = $<HTMLInputElement>('entry-creators');
const entryCreatorsSuggestions = $<HTMLUListElement>('entry-creators-suggestions');
const dizBannerTextArea = $<HTMLTextAreaElement>('diz-banner-text');
attachOverwriteGrid(dizBannerTextArea);

const pickFilesBtn = $<HTMLButtonElement>('pick-files-btn');
const selectedFilesList = $<HTMLUListElement>('selected-files-list');
const buildBtn = $<HTMLButtonElement>('build-btn');
const buildStatus = $<HTMLDivElement>('build-status');
const form = $<HTMLFormElement>('release-form');

const publishSection = $<HTMLElement>('publish-section');
const pickImageBtn = $<HTMLButtonElement>('pick-image-btn');
const selectedImageDiv = $<HTMLDivElement>('selected-image');
const publishBtn = $<HTMLButtonElement>('publish-btn');
const publishStatus = $<HTMLDivElement>('publish-status');

const memberlist = new MemberlistTable(membersTbody);
let nextReleaseNumber = 0;
let selectedPaths: string[] = [];
let lastBuiltCode: string | null = null;
let lastBuiltArchivePath: string | null = null;
let selectedImagePath: string | null = null;

attachCreatorsAutocomplete(entryCreatorsInput, entryCreatorsSuggestions, () => memberlist.getMembers());

function updateReleaseNumberPreview(): void {
  const code = codeInput.value.trim().toUpperCase() || 'XXXXX';
  const format = getSelectedFormat().toLowerCase();
  releaseNumberPreview.textContent = `release ${nextReleaseNumber} up-${code.toLowerCase()}.${format}`;
}

function getSelectedFormat(): ReleaseFormat {
  const checked = document.querySelector<HTMLInputElement>('input[name="format"]:checked');
  return (checked?.value as ReleaseFormat) ?? 'LHA';
}

/**
 * Resets everything related to the Publish section. Called at the start of
 * every new build attempt so a previous release's stale state (its success
 * message, its archive path wired to the Publish button, its picked cover
 * image) can never silently carry over to a different release's code/build.
 */
function resetPublishState(): void {
  lastBuiltCode = null;
  lastBuiltArchivePath = null;
  selectedImagePath = null;
  selectedImageDiv.textContent = '';
  publishSection.hidden = true;
  publishStatus.textContent = '';
  publishStatus.className = 'status';
  publishBtn.disabled = true;
}

/**
 * ipcMain.handle only forwards an Error's `message` string across the IPC
 * boundary — Electron's structured-clone serialization strips any other
 * properties from a thrown Error. For the 409 "already published" case,
 * ipc-handlers.ts's publishToWeb handler JSON-encodes {message, status,
 * wordpressPostUrl} into that message string; this reconstructs it. A
 * plain (non-JSON) message — every other failure — falls back to a bare
 * message with no status/wordpressPostUrl.
 */
function parsePublishError(err: unknown): { message: string; status?: number; wordpressPostUrl?: string } {
  const raw = err instanceof Error ? err.message : String(err);
  try {
    const parsed = JSON.parse(raw);
    if (parsed && typeof parsed === 'object' && typeof parsed.message === 'string') {
      return parsed as { message: string; status?: number; wordpressPostUrl?: string };
    }
  } catch {
    // not JSON — a plain error message, fall through
  }
  return { message: raw };
}

function renderSelectedFiles(): void {
  selectedFilesList.innerHTML = '';
  for (const p of selectedPaths) {
    const li = document.createElement('li');
    const span = document.createElement('span');
    span.textContent = p;
    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'danger';
    removeBtn.textContent = 'Remove';
    removeBtn.addEventListener('click', () => {
      selectedPaths = selectedPaths.filter((sp) => sp !== p);
      renderSelectedFiles();
    });
    li.append(span, removeBtn);
    selectedFilesList.appendChild(li);
  }
}

async function init(): Promise<void> {
  try {
    const seed = await window.api.loadSeedState();
    memberlist.seed(seed.members);
    nextReleaseNumber = seed.nextReleaseNumber;
    dizBannerTextArea.value = toGridText(seed.currentDizText);
    updateReleaseNumberPreview();
    if (seed.configWarning) {
      buildStatus.textContent = seed.configWarning;
      buildStatus.className = 'status warn';
    }
  } catch (err) {
    buildStatus.textContent = `Failed to load current members/release number: ${
      err instanceof Error ? err.message : String(err)
    }`;
    buildStatus.className = 'status warn';
  }
}

addMemberBtn.addEventListener('click', () => memberlist.addRow());

codeInput.addEventListener('input', () => {
  codeInput.value = codeInput.value.toUpperCase();
  updateReleaseNumberPreview();
});

document.querySelectorAll<HTMLInputElement>('input[name="format"]').forEach((el) => {
  el.addEventListener('change', updateReleaseNumberPreview);
});

let codeCheckTimer: number | undefined;
codeInput.addEventListener('input', () => {
  window.clearTimeout(codeCheckTimer);
  const code = codeInput.value.trim();
  codeStatus.textContent = '';
  codeStatus.className = 'status';
  if (code.length !== 5) return;
  codeCheckTimer = window.setTimeout(async () => {
    const { exists } = await window.api.checkCodeAvailability(code);
    if (exists) {
      codeStatus.textContent = `UP-${code.toUpperCase()} already exists on disk — building will fail unless you change the code.`;
      codeStatus.className = 'status warn';
    }
  }, 300);
});

pickFilesBtn.addEventListener('click', async () => {
  const paths = await window.api.pickFilesAndDirs();
  for (const p of paths) {
    if (!selectedPaths.includes(p)) selectedPaths.push(p);
  }
  renderSelectedFiles();
});

form.addEventListener('submit', async (e) => {
  e.preventDefault();

  // A new build attempt starts here, whether it's for a brand-new code or a
  // retry — any Publish state left over from a previous build must not
  // linger (see resetPublishState's doc comment).
  resetPublishState();

  const code = codeInput.value.trim().toUpperCase();
  if (!/^[A-Z0-9]{5}$/.test(code)) {
    buildStatus.textContent = 'Code must be exactly 5 letters/digits.';
    buildStatus.className = 'status warn';
    return;
  }
  if (selectedPaths.length === 0) {
    buildStatus.textContent = 'Select at least one file or folder to include.';
    buildStatus.className = 'status warn';
    return;
  }

  const formData: ReleaseFormData = {
    code,
    format: getSelectedFormat(),
    releaseName: releaseNameInput.value.trim(),
    platformTag: platformTagInput.value.trim(),
    releaseNotesText: releaseNotesTextArea.value,
    newsText: newsTextArea.value,
    dizBannerText: dizBannerTextArea.value,
    members: memberlist.getMembers(),
    entryTitle: entryTitleInput.value.trim(),
    entryCategory: entryCategoryInput.value.trim(),
    entryCreators: entryCreatorsInput.value.trim(),
    selectedPaths,
  };

  buildBtn.disabled = true;
  buildStatus.textContent = 'Building...';
  buildStatus.className = 'status';
  try {
    const result = await window.api.buildRelease(formData);
    buildStatus.textContent = `Built release ${result.releaseNumber}: ${result.archivePath}`;
    buildStatus.className = 'status ok';
    lastBuiltCode = code;
    lastBuiltArchivePath = result.archivePath;
    publishSection.hidden = false;
    publishBtn.disabled = !selectedImagePath;
  } catch (err) {
    buildStatus.textContent = `Build failed: ${err instanceof Error ? err.message : String(err)}`;
    buildStatus.className = 'status warn';
  } finally {
    buildBtn.disabled = false;
  }
});

pickImageBtn.addEventListener('click', async () => {
  const picked = await window.api.pickImage();
  if (picked) {
    selectedImagePath = picked;
    selectedImageDiv.textContent = picked;
    publishBtn.disabled = !lastBuiltArchivePath;
  }
});

publishBtn.addEventListener('click', async () => {
  if (!lastBuiltCode || !lastBuiltArchivePath || !selectedImagePath) return;
  publishBtn.disabled = true;
  publishStatus.textContent = 'Publishing... (this can take a while for a large archive, please wait)';
  publishStatus.className = 'status';
  try {
    const result = await window.api.publishToWeb(lastBuiltCode, lastBuiltArchivePath, selectedImagePath);
    publishStatus.textContent = `Published: ${result.wordpressPostUrl} (archive: ${result.archiveUrl})`;
    publishStatus.className = 'status ok';
    // A real, successful publish — leave publishBtn disabled. A stray
    // second click would just 409; nothing more needs to happen for this
    // build once it's published.
  } catch (err) {
    const parsed = parsePublishError(err);
    if (parsed.status === 409) {
      publishStatus.textContent = parsed.wordpressPostUrl
        ? `Already published: ${parsed.wordpressPostUrl}`
        : `Already published: ${parsed.message}`;
      publishStatus.className = 'status warn';
      // Already published — leave publishBtn disabled, same as the success
      // path. A genuinely-already-published release doesn't need retrying.
    } else {
      publishStatus.textContent = `Publish failed: ${parsed.message}`;
      publishStatus.className = 'status warn';
      // Any other failure (network error, 500, etc) is the genuine retry
      // case — re-enable so the user can try again.
      publishBtn.disabled = false;
    }
  }
});

init();
