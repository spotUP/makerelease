import { ReleaseFormat, ReleaseFormData } from '../shared/types';
import { MemberlistTable } from './memberlist-table';

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
const entryCategoryInput = $<HTMLInputElement>('entry-category');
const entryCreatorsInput = $<HTMLInputElement>('entry-creators');
const dizBannerTextArea = $<HTMLTextAreaElement>('diz-banner-text');
const pickFilesBtn = $<HTMLButtonElement>('pick-files-btn');
const selectedFilesList = $<HTMLUListElement>('selected-files-list');
const buildBtn = $<HTMLButtonElement>('build-btn');
const buildStatus = $<HTMLDivElement>('build-status');
const form = $<HTMLFormElement>('release-form');

const memberlist = new MemberlistTable(membersTbody);
let nextReleaseNumber = 0;
let selectedPaths: string[] = [];

function updateReleaseNumberPreview(): void {
  const code = codeInput.value.trim().toUpperCase() || 'XXXXX';
  const format = getSelectedFormat().toLowerCase();
  releaseNumberPreview.textContent = `release ${nextReleaseNumber} up-${code.toLowerCase()}.${format}`;
}

function getSelectedFormat(): ReleaseFormat {
  const checked = document.querySelector<HTMLInputElement>('input[name="format"]:checked');
  return (checked?.value as ReleaseFormat) ?? 'LHA';
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
  const seed = await window.api.loadSeedState();
  memberlist.seed(seed.members);
  nextReleaseNumber = seed.nextReleaseNumber;
  updateReleaseNumberPreview();
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
  } catch (err) {
    buildStatus.textContent = `Build failed: ${err instanceof Error ? err.message : String(err)}`;
    buildStatus.className = 'status warn';
  } finally {
    buildBtn.disabled = false;
  }
});

init();
