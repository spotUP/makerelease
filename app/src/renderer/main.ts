import { ReleaseFormat, ReleaseFormData } from '../shared/types';
import { MemberlistTable } from './memberlist-table.js';
import { CreditsTable } from './credits-table.js';
import { MediaEmbedsTable } from './media-embeds-table.js';
import { attachOverwriteGrid, toGridText } from './diz-grid.js';
import { attachCreatorsAutocomplete } from './creators-autocomplete.js';
import { describeRevoke } from './revoke-message.js';

const $ = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;

const codeInput = $<HTMLInputElement>('code');
const codeStatus = $<HTMLDivElement>('code-status');
const releaseNameInput = $<HTMLInputElement>('release-name');
const platformTagInput = $<HTMLInputElement>('platform-tag');
const descriptionInput = $<HTMLInputElement>('description-text');
const releaseNotesTextArea = $<HTMLTextAreaElement>('release-notes-text');
const newsTextArea = $<HTMLTextAreaElement>('news-text');
const membersTbody = $<HTMLTableSectionElement>('members-tbody');
const addMemberBtn = $<HTMLButtonElement>('add-member-btn');
const creditsTbody = $<HTMLTableSectionElement>('credits-tbody');
const addCreditBtn = $<HTMLButtonElement>('add-credit-btn');
const mediaEmbedsContainer = $<HTMLElement>('media-embeds-container');
const addMediaEmbedBtn = $<HTMLButtonElement>('add-media-embed-btn');
const releaseNumberPreview = $<HTMLDivElement>('release-number-preview');
const entryTitleInput = $<HTMLInputElement>('entry-title');
const entryCategoryInput = $<HTMLSelectElement>('entry-category');
const entryCreatorsInput = $<HTMLInputElement>('entry-creators');
const entryCreatorsSuggestions = $<HTMLUListElement>('entry-creators-suggestions');
const dizBannerTextArea = $<HTMLTextAreaElement>('diz-banner-text');
attachOverwriteGrid(dizBannerTextArea);

const pickFilesBtn = $<HTMLButtonElement>('pick-files-btn');
const selectedFilesList = $<HTMLUListElement>('selected-files-list');
const dryRunCheckbox = $<HTMLInputElement>('dry-run-checkbox');
const buildBtn = $<HTMLButtonElement>('build-btn');
const buildStatus = $<HTMLDivElement>('build-status');
const form = $<HTMLFormElement>('release-form');

const publishSection = $<HTMLElement>('publish-section');
const pickImageBtn = $<HTMLButtonElement>('pick-image-btn');
const selectedImageDiv = $<HTMLDivElement>('selected-image');
const publishBtn = $<HTMLButtonElement>('publish-btn');
const publishStatus = $<HTMLDivElement>('publish-status');

const revokeCodeInput = $<HTMLInputElement>('revoke-code');
const deleteReleaseBtn = $<HTMLButtonElement>('delete-release-btn');
const deleteStatus = $<HTMLDivElement>('delete-status');

const memberlist = new MemberlistTable(membersTbody);
const creditsTable = new CreditsTable(creditsTbody);
const mediaEmbedsTable = new MediaEmbedsTable(mediaEmbedsContainer, () => window.api.pickImage());
let nextReleaseNumber = 0;
let selectedPaths: string[] = [];
let lastBuiltCode: string | null = null;
let lastBuiltArchivePath: string | null = null;
let selectedImagePath: string | null = null;
// Captured at build-success time from mediaEmbedsTable, frozen for the
// later Publish click — mirrors lastBuiltArchivePath. The form's own
// media-embed rows could be edited (or cleared) between Build and Publish;
// Publish must send exactly what was actually built and sent to the server,
// not whatever the form currently shows.
let lastEmbedImagePaths: string[] = [];

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
 * Resets everything related to the Publish and cleanup sections. Called at
 * the start of every new build attempt so a previous release's stale state
 * (its success message, its archive path wired to the Publish button, its
 * picked cover image) can never silently carry over to a different
 * release's code/build.
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
  deleteReleaseBtn.disabled = false;
  deleteStatus.textContent = '';
  deleteStatus.className = 'status';
  lastEmbedImagePaths = [];
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
addCreditBtn.addEventListener('click', () => creditsTable.addRow());
addMediaEmbedBtn.addEventListener('click', () => mediaEmbedsTable.addRow());

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
  const mediaEmbedsError = mediaEmbedsTable.getValidationError();
  if (mediaEmbedsError) {
    buildStatus.textContent = mediaEmbedsError;
    buildStatus.className = 'status warn';
    return;
  }

  const mediaEmbeds = mediaEmbedsTable.getMediaEmbeds();
  const embedImagePaths = mediaEmbedsTable.getEmbedImagePaths();

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
    description: descriptionInput.value.trim(),
    credits: creditsTable.getCredits(),
    mediaEmbeds,
  };

  const dryRun = dryRunCheckbox.checked;

  buildBtn.disabled = true;
  buildStatus.textContent = dryRun ? 'Building (dry run)...' : 'Building...';
  buildStatus.className = 'status';
  try {
    const result = await window.api.buildRelease(formData, dryRun);
    if (result.isDryRun) {
      buildStatus.textContent =
        `Dry run built: ${result.archivePath} (preview release ${result.releaseNumber} — ` +
        'nothing was sent to the server, no release number was consumed).';
      buildStatus.className = 'status ok';
      // No real release row exists server-side for a dry run — Publish and
      // the cleanup/delete section both stay hidden (resetPublishState()
      // above already did this; nothing further to show here).
    } else {
      buildStatus.textContent =
        `Built release ${result.releaseNumber}: ${result.archivePath} — it is a draft until you ` +
        'publish it below, so nothing about it is public yet. Abandoning it costs nothing; use ' +
        'Revoke to hand the release number straight back.';
      buildStatus.className = 'status ok';
      lastBuiltCode = code;
      lastBuiltArchivePath = result.archivePath;
      lastEmbedImagePaths = embedImagePaths;
      publishSection.hidden = false;
      publishBtn.disabled = !selectedImagePath;
      // Pre-fill the revoke box with what was just built: that is the release
      // a mistake is most likely to be about, and it is the only one the
      // server will accept anyway until something newer exists.
      revokeCodeInput.value = code;
    }
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
    const result = await window.api.publishToWeb(
      lastBuiltCode,
      lastBuiltArchivePath,
      selectedImagePath,
      lastEmbedImagePaths,
    );
    if (result.ok) {
      publishStatus.textContent = `Published: ${result.wordpressPostUrl} (archive: ${result.archiveUrl})`;
      publishStatus.className = 'status ok';
      // A real, successful publish — leave publishBtn disabled. A stray
      // second click would just 409; nothing more needs to happen for this
      // build once it's published.
    } else if (result.status === 409 && result.reason === 'number-taken') {
      // The site already has a post with this release number, so the
      // catalogue and the website disagree about which numbers are taken.
      // Retrying changes nothing until a human reconciles them, but it is
      // worth retrying afterwards, so the button stays live.
      publishStatus.textContent = `Not published — ${result.message}`;
      publishStatus.className = 'status warn';
      publishBtn.disabled = false;
    } else if (result.status === 409) {
      publishStatus.textContent = result.wordpressPostUrl
        ? `Already published: ${result.wordpressPostUrl}`
        : `Already published: ${result.message}`;
      publishStatus.className = 'status warn';
      // Already published — leave publishBtn disabled, same as the success
      // path. A genuinely-already-published release doesn't need retrying.
    } else {
      publishStatus.textContent = `Publish failed: ${result.message}`;
      publishStatus.className = 'status warn';
      // Any other failure (network error, 500, etc) is the genuine retry
      // case — re-enable so the user can try again.
      publishBtn.disabled = false;
    }
  } catch (err) {
    // Only a genuinely unexpected failure (a bug, not a modeled outcome)
    // reaches here — the IPC handler returns a discriminated result for
    // every known failure mode instead of throwing.
    publishStatus.textContent = `Publish failed: ${err instanceof Error ? err.message : String(err)}`;
    publishStatus.className = 'status warn';
    publishBtn.disabled = false;
  }
});

deleteReleaseBtn.addEventListener('click', async () => {
  // Read from the input, not from this session's build: a release that needs
  // revoking is often one from an earlier run of the app, after the mistake
  // has been noticed. Requiring a fresh build to reach the button left those
  // unreachable, which is exactly how two abandoned builds sat in the public
  // catalogue.
  const deletedCode = revokeCodeInput.value.trim().toUpperCase();
  if (!deletedCode) {
    deleteStatus.textContent = 'Enter the code of the release to revoke.';
    deleteStatus.className = 'status warn';
    return;
  }
  const confirmed = window.confirm(
    `Revoke release ${deletedCode}? This removes its catalog entry, frees its release number ` +
      'and its code, deletes the published archive, and moves its WordPress post to the trash. ' +
      "It only works if it's still the most recently created release.",
  );
  if (!confirmed) return;

  deleteReleaseBtn.disabled = true;
  deleteStatus.textContent = 'Revoking...';
  deleteStatus.className = 'status';
  try {
    const result = await window.api.deleteRelease(deletedCode);
    if (result.ok) {
      deleteStatus.textContent = describeRevoke(result);
      // Anything the server could not finish itself (a post it failed to
      // trash, an archive it failed to unlink) is the user's to follow up,
      // so it reads as a warning rather than a clean success.
      deleteStatus.className =
        result.wordpressPostError !== null || (result.wasPublished && !result.archiveRemoved)
          ? 'status warn'
          : 'status ok';
      revokeCodeInput.value = '';
      // Revoking the release this session just built leaves nothing to
      // publish, so the publish state goes with it — a stray click on Publish
      // must not fire against a code that is gone. Revoking some older
      // release leaves this session's own build alone. deleteStatus stays as
      // it is either way: it may carry a WordPress-post URL the user still
      // has to act on.
      if (deletedCode === lastBuiltCode) {
        lastBuiltCode = null;
        lastBuiltArchivePath = null;
        lastEmbedImagePaths = [];
        publishSection.hidden = true;
        publishStatus.textContent = '';
        publishStatus.className = 'status';
        publishBtn.disabled = true;
      }
      deleteReleaseBtn.disabled = false;
    } else if (result.status === 409) {
      deleteStatus.textContent = `Could not revoke: ${result.message} (a newer release exists — this is no longer the latest).`;
      deleteStatus.className = 'status warn';
      deleteReleaseBtn.disabled = false;
    } else {
      deleteStatus.textContent = `Revoke failed: ${result.message}`;
      deleteStatus.className = 'status warn';
      deleteReleaseBtn.disabled = false;
    }
  } catch (err) {
    deleteStatus.textContent = `Revoke failed: ${err instanceof Error ? err.message : String(err)}`;
    deleteStatus.className = 'status warn';
    deleteReleaseBtn.disabled = false;
  }
});

init();
