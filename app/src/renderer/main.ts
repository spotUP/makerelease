import { AminetUploadRequest, ReleaseFormat, ReleaseFormData } from '../shared/types';
import type { ProfileSummary } from '../shared/profile';
import { archiveFileName, releaseBaseName } from '../shared/naming.js';
import {
  AMINET_ARCHITECTURES,
  AMINET_SHORT_MAX,
  aminetReadmeFieldProblems,
  type AminetReadmeField,
  renderAminetReadme,
} from '../shared/aminet-readme.js';
import { MemberlistTable } from './memberlist-table.js';
import { BoardsTable } from './boards-table.js';
import { CreditsTable } from './credits-table.js';
import { CODER_ROLES, DESIGN_ROLES, creatorsFromCredits, creditNamesForRole, creditsError } from './credits-creators.js';
import { MediaEmbedsTable } from './media-embeds-table.js';
import { createThumb } from './image-thumb.js';
import { applyReleaseLines, findReleaseLineSlot, readReleaseLineText } from '../shared/diz-release-line.js';
import { DEFAULT_DIVISION, divisionOf, isDivisionId, type DivisionId } from '../shared/divisions.js';
import { suggestReleaseCode } from '../shared/code-suggest.js';
import { todayIso } from '../shared/release-date.js';
import { describeRevoke } from './revoke-message.js';
import { builtReleaseFor, type BuiltRelease } from './built-release.js';
import { LatestOnly } from './latest-only.js';
import { Prefill, releaseNameText } from './prefill.js';
import { wrapWords } from '../shared/text-wrap.js';
import { clearFieldError, clearFieldErrors, nativeFieldErrors, showFieldErrors, type FieldError } from './field-errors.js';
import { toast } from './toast.js';
import { AMINET_TYPES } from '../shared/aminet-types.js';
import { guessAminetType } from '../shared/aminet-guess.js';
import { applyFoldDefaults, initWizard } from './wizard.js';
import { createProfileEditor, type ProfileEditorMode } from './profile-editor.js';
import { visibleSections, type SectionVisibility } from '../shared/section-visibility.js';

const $ = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;

const codeInput = $<HTMLInputElement>('code');
const codeLabel = $<HTMLLabelElement>('code-label');
const profileSelect = $<HTMLSelectElement>('profile-select');
const openProfilesBtn = $<HTMLButtonElement>('open-profiles-btn');
const reloadProfilesBtn = $<HTMLButtonElement>('reload-profiles-btn');
const profileStatus = $<HTMLDivElement>('profile-status');
const editProfileBtn = $<HTMLButtonElement>('edit-profile-btn');
const duplicateProfileBtn = $<HTMLButtonElement>('duplicate-profile-btn');
const newProfileBtn = $<HTMLButtonElement>('new-profile-btn');
const deleteProfileBtn = $<HTMLButtonElement>('delete-profile-btn');
const codeStatus = $<HTMLDivElement>('code-status');
const releaseNameInput = $<HTMLInputElement>('release-name');
const platformTagInput = $<HTMLSelectElement>('platform-tag');
const platformTagOtherInput = $<HTMLInputElement>('platform-tag-other');
// Console and Other ask which one in free text; an empty Console stays "Console".
const PLATFORM_FREE_TEXT: Record<string, { label: string; placeholder: string }> = {
  Console: { label: 'Which console', placeholder: 'Mega Drive' },
  other: { label: 'Other platform tag', placeholder: 'CD32' },
};
platformTagInput.addEventListener('change', () => {
  const free = PLATFORM_FREE_TEXT[platformTagInput.value];
  platformTagOtherInput.hidden = !free;
  if (!free) return;
  platformTagOtherInput.placeholder = free.placeholder;
  platformTagOtherInput.setAttribute('aria-label', free.label);
  platformTagOtherInput.focus();
});
function platformTagText(): string {
  const text = platformTagOtherInput.value.trim();
  if (platformTagInput.value === 'other') return text;
  if (platformTagInput.value === 'Console') return text || 'Console';
  return platformTagInput.value;
}
const descriptionInput = $<HTMLInputElement>('description-text');
const releaseNotesTextArea = $<HTMLTextAreaElement>('release-notes-text');
const newsTextArea = $<HTMLTextAreaElement>('news-text');
const membersTbody = $<HTMLTableSectionElement>('members-tbody');
const addMemberBtn = $<HTMLButtonElement>('add-member-btn');
const creditsTbody = $<HTMLTableSectionElement>('credits-tbody');
const boardsTbody = $<HTMLTableSectionElement>('boards-tbody');
const addBoardBtn = $<HTMLButtonElement>('add-board-btn');
const xFieldsSection = $<HTMLElement>('x-fields-section');
const xSystemInput = $<HTMLInputElement>('x-system');
const xCreditsPreview = $<HTMLDivElement>('x-credits-preview');
const addCreditBtn = $<HTMLButtonElement>('add-credit-btn');
const mediaEmbedsContainer = $<HTMLElement>('media-embeds-container');
const addMediaEmbedBtn = $<HTMLButtonElement>('add-media-embed-btn');
const releaseNumberPreview = $<HTMLDivElement>('release-number-preview');
const entryCategoryInput = $<HTMLSelectElement>('entry-category');
const releaseDateInput = $<HTMLInputElement>('release-date');
const releasedAtInput = $<HTMLInputElement>('released-at');
const releaseNumberTextInput = $<HTMLInputElement>('release-number-text');
const releaseNumberSection = $<HTMLElement>('section-release-number');
const historyEntrySection = $<HTMLElement>('section-history-entry');
const mediaEmbedsSection = $<HTMLElement>('section-media-embeds');
const dizLineSection = $<HTMLElement>('section-diz-line');
const dizReleaseLineHint = $<HTMLElement>('diz-release-line-hint');
const cleanupSection = $<HTMLElement>('cleanup-section');
const creatorsPreview = $<HTMLDivElement>('creators-preview');
const dizBannerPreview = $<HTMLPreElement>('diz-banner-preview');
const nestedWarning = $<HTMLDivElement>('nested-warning');
const archivePreview = $<HTMLPreElement>('archive-preview');
const nfoPreview = $<HTMLPreElement>('nfo-preview');
const nfoPreviewStatus = $<HTMLDivElement>('nfo-preview-status');
const dizReleaseLinesContainer = $<HTMLDivElement>('diz-release-lines');
const templateStatus = $<HTMLDivElement>('template-status');

const pickFilesBtn = $<HTMLButtonElement>('pick-files-btn');
const selectedFilesList = $<HTMLUListElement>('selected-files-list');
const previewBtn = $<HTMLButtonElement>('preview-btn');
const buildBtn = $<HTMLButtonElement>('build-btn');
const buildStatus = $<HTMLDivElement>('build-status');
const buildProgress = $<HTMLDivElement>('build-progress');
const buildProgressStage = $<HTMLSpanElement>('build-progress-stage');
const form = $<HTMLFormElement>('release-form');

const publishSection = $<HTMLElement>('publish-section');
const pickImageBtn = $<HTMLButtonElement>('pick-image-btn');
const selectedImageDiv = $<HTMLDivElement>('selected-image');
const publishBtn = $<HTMLButtonElement>('publish-btn');
const publishStatus = $<HTMLDivElement>('publish-status');
const aminetCheckbox = $<HTMLInputElement>('aminet-checkbox');
const aminetShortInput = $<HTMLInputElement>('aminet-short');
const aminetTypeInput = $<HTMLInputElement>('aminet-type');
const aminetTypeList = $<HTMLDataListElement>('aminet-type-list');
const aminetArchitectureInput = $<HTMLInputElement>('aminet-architecture');
const aminetArchitectureList = $<HTMLDataListElement>('aminet-architecture-list');
const aminetUploaderNameInput = $<HTMLInputElement>('aminet-uploader-name');
const aminetUploaderEmailInput = $<HTMLInputElement>('aminet-uploader-email');
const aminetReadmePreview = $<HTMLPreElement>('aminet-readme-preview');
const aminetReadmeProblemsDiv = $<HTMLDivElement>('aminet-readme-problems');
const aminetSection = $<HTMLElement>('section-aminet');
const aminetUploadBtn = $<HTMLButtonElement>('aminet-upload-btn');
const aminetStatus = $<HTMLDivElement>('aminet-status');

const revokeCodeInput = $<HTMLInputElement>('revoke-code');
const deleteReleaseBtn = $<HTMLButtonElement>('delete-release-btn');
const deleteStatus = $<HTMLDivElement>('delete-status');

const memberlist = new MemberlistTable(membersTbody);
// Which form sections the chosen group sees; replaced by applyProfile().
let visibility: SectionVisibility = visibleSections({ renderer: DEFAULT_DIVISION, backend: 'releases-doorserver' });
const creditsTable = new CreditsTable(creditsTbody, {
  getMembers: () => (visibility.creatorsAutocomplete ? memberlist.getMembers() : []),
  onChange: () => updateCreatorsPreview(),
});
const boardsTable = new BoardsTable(boardsTbody);
const mediaEmbedsTable = new MediaEmbedsTable(mediaEmbedsContainer, () => window.api.pickImage());
let nextReleaseNumber = 0;
let selectedPaths: string[] = [];
let built: BuiltRelease | null = null;
let selectedImagePath: string | null = null;

// Typing in a credit row has to refresh the derived creator line too, and
// rows come and go - one delegated listener on the table body covers every
// row that will ever exist, which per-input listeners would not.
creditsTbody.addEventListener('input', () => updateCreatorsPreview());

// The release name is also the history entry title and the diz release line
// ("<name> at <place>" when Released at is filled).
// The description is not prefilled: the post title and the NFO already print
// the name, so the description holds only what comes after it.

function dizLineSourceText(): string {
  const name = releaseNameText(releaseNameInput.value);
  const place = releasedAtInput.value.trim();
  return name && place ? `${name} at ${place}` : name;
}
releasedAtInput.addEventListener('input', () => renderDizPreview());


const codePrefill = new Prefill();

releaseNameInput.addEventListener('input', () => {
  const name = releaseNameText(releaseNameInput.value);
  const code = codePrefill.next(codeInput.value, suggestReleaseCode(name, currentProfile?.codeLength ?? 5));
  if (code !== undefined && code !== codeInput.value) {
    codeInput.value = code;
    codeInput.dispatchEvent(new Event('input'));
  }
  renderDizPreview();
  refreshAminetPreview();
});

// Banner art is read-only: whatever the last release used (or the built-in
// fallback when there is no previous release). Only the release line changes,
// and diz-release-line.ts keeps it inside the art's frame characters. How
// many of those lines a release may retype is the division's business - Up
// Rough names itself on one, /X adds a tagline under it.
let currentDivision: DivisionId = DEFAULT_DIVISION;
// The group chosen in the dropdown; null until the first seed has loaded.
let currentProfile: ProfileSummary | null = null;
let dizBannerTemplate = divisionOf(currentDivision).defaultDizBanner;

/** Each editable row's text: the release name cut to the slot, or the art's own line until there is a name. */
function dizLineTexts(): string[] {
  const source = dizLineSourceText();
  return divisionOf(currentDivision).editableDizRows.map((spec) =>
    (source || readReleaseLineText(dizBannerTemplate, spec.row)).slice(0, findReleaseLineSlot(dizBannerTemplate, spec.row).width),
  );
}

function currentDizBannerText(): string {
  const texts = dizLineTexts();
  return applyReleaseLines(
    dizBannerTemplate,
    divisionOf(currentDivision).editableDizRows.map((spec, i) => ({ row: spec.row, text: texts[i] })),
  );
}

function renderDizPreview(): void {
  dizBannerPreview.textContent = currentDizBannerText();
  const line = dizLineSourceText();
  dizReleaseLinesContainer.textContent = divisionOf(currentDivision)
    .editableDizRows.map((spec) => {
      const width = findReleaseLineSlot(dizBannerTemplate, spec.row).width;
      return line.length > width
        ? `${spec.label}: ${line.length} characters, the diz fits ${width}; the rest is cut off.`
        : `${spec.label}: ${line.length}/${width} characters.`;
    })
    .join('\n');
}

function seedDizBanner(bannerText: string): void {
  const division = divisionOf(currentDivision);
  // A division with fixed art always starts from its own template; only one
  // that inherits takes the previous release's drawing.
  dizBannerTemplate =
    !division.inheritsBannerArt || bannerText.trim() === '' ? division.defaultDizBanner : bannerText;
  renderDizPreview();
}

// The creator line the NFO history entry will carry, shown while the credits
// are typed so nobody has to build to find out what it says.
function updateCreatorsPreview(): void {
  const credits = creditsTable.getCredits();
  const creators = creatorsFromCredits(credits);
  // The /X NFO's bracketed header fields come from the same credits, so show
  // what they will say rather than making anybody build to find out.
  const coder = creditNamesForRole(credits, CODER_ROLES);
  const design = creditNamesForRole(credits, DESIGN_ROLES);
  xCreditsPreview.textContent =
    `NFO header: cODER [${coder || '-'}]  dESIGN [${design || '-'}]` +
    (coder || design ? '' : ' - add credits with roles like Code or Graphics');
  creatorsPreview.textContent = creators
    ? `Release history creator line: ${creators}`
    : 'Release history creator line: add a credit with both a role and a name.';
}

function updateReleaseNumberPreview(): void {
  const code = codeInput.value.trim().toUpperCase() || 'X'.repeat(currentProfile?.codeLength ?? 5);
  const fileName = archiveFileName(currentProfile ?? { filePrefix: '' }, code, getSelectedFormat()).toLowerCase();
  releaseNumberPreview.textContent = `release ${nextReleaseNumber} ${fileName}`;
}

function getSelectedFormat(): ReleaseFormat {
  const checked = document.querySelector<HTMLInputElement>('input[name="format"]:checked');
  return (checked?.value as ReleaseFormat) ?? 'LHA';
}

// ---- Aminet ---------------------------------------------------------------
//
// The .readme is composed here, from the same form fields the NFO is built
// from, and previewed live; the exact previewed text is what Publish sends
// to main for upload. Who uploads (name, email) and where (directory,
// architecture) hardly change between releases, so they are remembered in
// localStorage - the one per-user setting this app has outside the server.

const AMINET_STORAGE_KEY = 'aminet-uploader';

interface AminetRemembered {
  uploaderName: string;
  uploaderEmail: string;
  type: string;
  architecture: string;
}

function loadAminetRemembered(): Partial<AminetRemembered> {
  try {
    const raw = localStorage.getItem(AMINET_STORAGE_KEY);
    if (!raw) return {};
    const parsed: unknown = JSON.parse(raw);
    return parsed && typeof parsed === 'object' ? (parsed as Partial<AminetRemembered>) : {};
  } catch {
    return {};
  }
}

function saveAminetRemembered(): void {
  const remembered: AminetRemembered = {
    uploaderName: aminetUploaderNameInput.value,
    uploaderEmail: aminetUploaderEmailInput.value,
    type: aminetTypeInput.value,
    architecture: aminetArchitectureInput.value,
  };
  try {
    localStorage.setItem(AMINET_STORAGE_KEY, JSON.stringify(remembered));
  } catch {
    // Storage full or disabled: the fields still work for this session.
  }
}

function fillDatalist(list: HTMLDataListElement, values: readonly (string | { type: string; description: string })[]): void {
  list.replaceChildren(
    ...values.map((value) => {
      const option = document.createElement('option');
      if (typeof value === 'string') option.value = value;
      else {
        option.value = value.type;
        option.label = value.description;
      }
      return option;
    }),
  );
}

function aminetReadmeText(): string {
  return renderAminetReadme({
    short: aminetShortInput.value.trim(),
    type: aminetTypeInput.value.trim(),
    architecture: aminetArchitectureInput.value.trim(),
    uploaderEmail: aminetUploaderEmailInput.value.trim(),
    uploaderName: aminetUploaderNameInput.value.trim(),
    group: { label: currentProfile?.label ?? '', url: currentProfile?.url ?? '' },
    entryCreators: creatorsFromCredits(creditsTable.getCredits()),
    description: descriptionInput.value.trim(),
    releaseNotesText: releaseNotesTextArea.value,
    credits: creditsTable.getCredits(),
  });
}

/** Re-renders the preview and lists what the Aminet checklist still misses. */
function refreshAminetPreview(): string[] {
  const text = aminetReadmeText();
  aminetReadmePreview.textContent = text;
  const problems = aminetReadmeFieldProblems(text).map((p) => p.message);
  aminetReadmeProblemsDiv.textContent = problems.length ? `Aminet readme: ${problems.join('; ')}` : 'Aminet readme is complete.';
  aminetReadmeProblemsDiv.className = problems.length ? 'status warn' : 'status ok';
  return problems;
}

/** The input that fixes each readme problem; whole-text problems point at the preview. */
function aminetFieldFor(field: AminetReadmeField): HTMLElement {
  switch (field) {
    case 'short':
      return aminetShortInput;
    case 'uploader':
      return aminetUploaderEmailInput;
    case 'type':
      return aminetTypeInput;
    case 'architecture':
      return aminetArchitectureInput;
    default:
      return aminetReadmePreview;
  }
}

/** Shows the readme problems under their Aminet inputs; true when there are none. */
function validateAminetFields(): boolean {
  refreshAminetPreview();
  const errors = aminetReadmeFieldProblems(aminetReadmeText()).map((p) => ({ field: aminetFieldFor(p.field), message: `${p.message}.` }));
  return showFieldErrors(aminetSection, errors);
}

/** What the Upload to Aminet step sends: exactly the previewed readme. */
function aminetRequest(): AminetUploadRequest {
  return { readmeText: aminetReadmeText(), uploaderEmail: aminetUploaderEmailInput.value.trim() };
}

/**
 * Called after a successful build: the description is usually the best
 * Short: there is, but only when it fits Aminet's 40 columns; otherwise the
 * release name, cut at a word boundary, so the field never starts empty.
 */
function prefillAminetShort(): void {
  if (aminetShortInput.value.trim()) return;
  const description = descriptionInput.value.trim();
  if (description && description.length <= AMINET_SHORT_MAX) aminetShortInput.value = description;
  else {
    const name = releaseNameText(releaseNameInput.value);
    aminetShortInput.value = (wrapWords(name, AMINET_SHORT_MAX, AMINET_SHORT_MAX)[0] ?? '').slice(0, AMINET_SHORT_MAX);
  }
}

/**
 * Uploads the last real build to Aminet. Both the Upload button and the
 * website Publish (when its Aminet box is ticked) end up here, so a failed
 * upload is retried with the same button whichever started it.
 */
async function uploadLastBuildToAminet(): Promise<void> {
  const release = builtReleaseFor(built, profileSelect.value);
  if (!release) return;
  if (!validateAminetFields()) {
    aminetStatus.textContent = 'Not uploaded - fix the Aminet fields marked above.';
    aminetStatus.className = 'status warn';
    return;
  }
  aminetUploadBtn.disabled = true;
  aminetStatus.textContent = 'Uploading to Aminet...';
  aminetStatus.className = 'status';
  try {
    const result = await window.api.uploadToAminet(aminetRequest(), release.archivePath);
    aminetStatus.textContent = `Aminet: ${result.message}.`;
    aminetStatus.className = result.ok ? 'status ok' : 'status warn';
    toast(result.ok ? 'ok' : 'error', result.ok ? 'Uploaded to Aminet.' : `Aminet upload failed: ${result.message}`);
    // Only a failure is worth a second click; a second upload of the same
    // file would just sit in Aminet's queue twice.
    aminetUploadBtn.disabled = result.ok;
  } catch (err) {
    aminetStatus.textContent = `Aminet upload failed: ${err instanceof Error ? err.message : String(err)}`;
    aminetStatus.className = 'status warn';
    toast('error', aminetStatus.textContent);
    aminetUploadBtn.disabled = false;
  }
}

{
  fillDatalist(aminetTypeList, AMINET_TYPES);
  fillDatalist(aminetArchitectureList, AMINET_ARCHITECTURES);
  const remembered = loadAminetRemembered();
  aminetUploaderNameInput.value = remembered.uploaderName ?? '';
  aminetUploaderEmailInput.value = remembered.uploaderEmail ?? '';
  aminetTypeInput.value = remembered.type ?? '';
  // The remembered directory was last release's guess, so it follows Category and Platform until edited.
  const aminetTypePrefill = new Prefill(aminetTypeInput.value);
  const followAminetType = (): void => {
    const guess = guessAminetType(entryCategoryInput.value, platformTagInput.value);
    if (!guess) return;
    const type = aminetTypePrefill.next(aminetTypeInput.value, guess);
    if (type === undefined || type === aminetTypeInput.value) return;
    aminetTypeInput.value = type;
    aminetTypeInput.dispatchEvent(new Event('input'));
  };
  entryCategoryInput.addEventListener('change', followAminetType);
  platformTagInput.addEventListener('change', followAminetType);
  aminetArchitectureInput.value = remembered.architecture ?? AMINET_ARCHITECTURES[0];
  for (const input of [aminetUploaderNameInput, aminetUploaderEmailInput, aminetTypeInput, aminetArchitectureInput]) {
    input.addEventListener('input', () => {
      saveAminetRemembered();
      refreshAminetPreview();
    });
  }
  aminetShortInput.addEventListener('input', () => refreshAminetPreview());
  // The body of the readme follows the form, so a late edit to the
  // description, notes or credits must show up in the preview too.
  for (const input of [descriptionInput, releaseNotesTextArea]) {
    input.addEventListener('input', () => refreshAminetPreview());
  }
  creditsTbody.addEventListener('input', () => refreshAminetPreview());
}

/**
 * Resets everything related to the Publish and cleanup sections. Called at
 * the start of every new build attempt so a previous release's stale state
 * (its success message, its archive path wired to the Publish button, its
 * picked cover image) can never silently carry over to a different
 * release's code/build.
 */
function resetPublishState(): void {
  built = null;
  selectedImagePath = null;
  selectedImageDiv.textContent = '';
  publishSection.hidden = true;
  publishStatus.textContent = '';
  publishStatus.className = 'status';
  publishBtn.disabled = true;
  aminetSection.hidden = true;
  aminetStatus.textContent = '';
  aminetStatus.className = 'status';
  aminetUploadBtn.disabled = true;
  clearFieldErrors(publishSection);
  clearFieldErrors(aminetSection);
  deleteReleaseBtn.disabled = false;
  deleteStatus.textContent = '';
  deleteStatus.className = 'status';
}

let archiveRequest = 0;
async function refreshArchivePreview(): Promise<void> {
  const mine = ++archiveRequest;
  nestedWarning.replaceChildren();
  nestedWarning.hidden = true;
  if (!currentProfile || selectedPaths.length === 0) {
    archivePreview.textContent = '';
    return;
  }
  const result = await window.api.previewArchive(
    readForm(codeInput.value.trim().toUpperCase() || 'X'.repeat(currentProfile.codeLength ?? 5)),
  );
  if (mine !== archiveRequest) return;
  archivePreview.textContent = result.entries.join('\n');
  for (const n of result.nested) {
    nestedWarning.hidden = false;
    const text = document.createElement('span');
    text.textContent = `Extra nested folder: ${n.path} only wraps ${n.flattenTo}. `;
    const fix = document.createElement('button');
    fix.type = 'button';
    fix.textContent = 'Remove extra nested folder';
    fix.addEventListener('click', () => {
      selectedPaths = selectedPaths.map((sp) => (sp === n.path ? n.flattenTo : sp));
      renderSelectedFiles();
    });
    nestedWarning.append(text, fix, document.createElement('br'));
  }
}

function renderSelectedFiles(): void {
  void refreshArchivePreview();
  // A button fires no input event, so its "choose a file" message is cleared here.
  if (selectedPaths.length) clearFieldError(pickFilesBtn);
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
    const thumb = createThumb(p);
    li.append(...(thumb ? [thumb] : []), span, removeBtn);
    selectedFilesList.appendChild(li);
  }
}

// Switching group is a different release entirely: its own numbering, its
// own roster, its own banner art. Reloading the seed is what makes all three
// follow, rather than each being patched in its own place.
const PROFILE_STORAGE_KEY = 'profile-id';

function rememberProfileId(id: string): void {
  try {
    localStorage.setItem(PROFILE_STORAGE_KEY, id);
  } catch {
    // Storage disabled: the choice still holds for this session.
  }
}

function rememberedProfileId(): string | null {
  try {
    return localStorage.getItem(PROFILE_STORAGE_KEY);
  } catch {
    return null;
  }
}

async function loadProfiles(): Promise<void> {
  profileStatus.textContent = '';
  profileStatus.className = 'status';
  try {
    const entries = await window.api.listProfiles();
    const usable = entries.filter((entry): entry is ProfileSummary => !('error' in entry));
    const broken = entries.filter((entry): entry is { id: string; error: string } => 'error' in entry);
    profileSelect.replaceChildren(
      ...usable.map((profile) => {
        const option = document.createElement('option');
        option.value = profile.id;
        option.textContent = profile.label;
        return option;
      }),
    );
    // A broken profile is listed, never hidden: the maintainer needs to
    // know why the group they just added is missing from the dropdown.
    if (broken.length > 0) {
      profileStatus.textContent = broken.map((entry) => `Profile "${entry.id}" not loaded: ${entry.error}`).join(' ');
      profileStatus.className = 'status warn';
    }
    if (usable.length === 0) {
      if (broken.length === 0) {
        profileStatus.textContent = 'No group profiles yet. Open the profiles folder and add one.';
        profileStatus.className = 'status warn';
      }
      return;
    }
    const remembered = rememberedProfileId();
    profileSelect.value = usable.some((profile) => profile.id === remembered) ? (remembered as string) : usable[0].id;
    await init();
  } catch (err) {
    profileStatus.textContent = `Could not list group profiles: ${err instanceof Error ? err.message : String(err)}`;
    profileStatus.className = 'status warn';
  }
}

profileSelect.addEventListener('change', () => {
  rememberProfileId(profileSelect.value);
  void init();
});

openProfilesBtn.addEventListener('click', async () => {
  try {
    await window.api.openProfilesFolder();
  } catch (err) {
    profileStatus.textContent = err instanceof Error ? err.message : String(err);
    profileStatus.className = 'status warn';
  }
});

reloadProfilesBtn.addEventListener('click', () => void loadProfiles());

const profileEditor = createProfileEditor();

async function editProfile(mode: ProfileEditorMode): Promise<void> {
  try {
    const takenIds = Array.from(profileSelect.options, (option) => option.value);
    const savedId = await profileEditor.open(mode, profileSelect.value || null, takenIds);
    if (savedId === null) return;
    rememberProfileId(savedId);
    await loadProfiles();
  } catch (err) {
    profileStatus.textContent = err instanceof Error ? err.message : String(err);
    profileStatus.className = 'status warn';
  }
}

newProfileBtn.addEventListener('click', () => void editProfile('new'));
editProfileBtn.addEventListener('click', () => void editProfile('edit'));
duplicateProfileBtn.addEventListener('click', () => void editProfile('duplicate'));

deleteProfileBtn.addEventListener('click', async () => {
  const id = profileSelect.value;
  const label = profileSelect.selectedOptions[0]?.textContent ?? id;
  if (!id || !window.confirm(`Move the group "${label}" to the Trash?`)) return;
  try {
    const outcome = await window.api.deleteProfile(id);
    if (!outcome.ok) {
      profileStatus.textContent = outcome.error;
      profileStatus.className = 'status warn';
      return;
    }
    await loadProfiles();
  } catch (err) {
    profileStatus.textContent = err instanceof Error ? err.message : String(err);
    profileStatus.className = 'status warn';
  }
});

function applyProfile(profile: ProfileSummary): void {
  currentProfile = profile;
  currentDivision = isDivisionId(profile.renderer) ? profile.renderer : DEFAULT_DIVISION;
  codeInput.maxLength = profile.codeLength;
  restoreFormat(profile.id);
  revokeCodeInput.maxLength = profile.codeLength;
  codeLabel.textContent =
    `${profile.codeLength}-character code - becomes ${releaseBaseName(profile, '<CODE>')}`;
  // The restored archive format changes a fold summary, and a new group may leave required fields empty.
  applyFoldDefaults();
}

/**
 * Hides what the chosen group has no use for. A hidden section's required
 * fields stop being required, or the browser would refuse to submit a form
 * whose missing field nobody can see.
 */
function applyVisibility(v: SectionVisibility): void {
  visibility = v;
  xFieldsSection.hidden = !v.xFields;
  historyEntrySection.hidden = !v.historyEntry;
  entryCategoryInput.required = v.historyEntry;
  releaseNumberPreview.hidden = !v.releaseNumberPreview;
  releaseNumberSection.hidden = !v.releaseNumberText;
  mediaEmbedsSection.hidden = !v.mediaEmbeds;
  dizLineSection.hidden = !v.dizReleaseLine && !v.dizPreviewOnly;
  dizReleaseLinesContainer.hidden = !v.dizReleaseLine;
  dizReleaseLineHint.hidden = !v.dizReleaseLine;
  templateStatus.hidden = !v.dizPreviewOnly;
  cleanupSection.hidden = !v.revoke;
  aminetCheckbox.parentElement!.hidden = !v.publishWebsite;
}

const initSwitches = new LatestOnly();

async function init(): Promise<void> {
  // A build belongs to the profile that made it; once the profile is switched
  // or reloaded, its Publish and Aminet state must not stay on screen.
  resetPublishState();
  const isLatest = initSwitches.begin();
  try {
    const seed = await window.api.loadSeedState(profileSelect.value);
    // A newer switch started while this seed loaded; its result wins.
    if (!isLatest()) return;
    applyProfile(seed.profile);
    // Today, unless somebody has already typed a date - reloading the seed
    // (which switching division does) must not overwrite what they entered.
    if (releaseDateInput.value.trim() === '') releaseDateInput.value = todayIso();
    boardsTable.seed(seed.boards);
    memberlist.seed(seed.members);
    nextReleaseNumber = seed.nextReleaseNumber;
    seedDizBanner(seed.currentDizText);
    // After the banner, which rebuilds the release-line inputs whose
    // required flag this sets.
    applyVisibility(visibleSections(seed.profile));
    scheduleTemplatePreview();
    updateReleaseNumberPreview();
    // Both can be set at once (an unconfigured build against a drifted
    // catalogue), and neither must hide the other.
    const warnings = [seed.numberingWarning, seed.staleDraftWarning, seed.configWarning].filter(Boolean);
    if (warnings.length > 0) {
      buildStatus.textContent = warnings.join(' ');
      buildStatus.className = 'status warn';
    }
  } catch (err) {
    buildStatus.textContent = `Failed to load current members/release number: ${
      err instanceof Error ? err.message : String(err)
    }`;
    buildStatus.className = 'status warn';
  }
}

// Show the fallback art immediately - init() re-seeds it once the server
// answers, and a failed seed load still leaves a usable, framed banner.
seedDizBanner('');
updateCreatorsPreview();

addMemberBtn.addEventListener('click', () => memberlist.addRow());
addCreditBtn.addEventListener('click', () => creditsTable.addRow());
addBoardBtn.addEventListener('click', () => boardsTable.addRow());
addMediaEmbedBtn.addEventListener('click', () => mediaEmbedsTable.addRow());

codeInput.addEventListener('input', () => {
  codeInput.value = codeInput.value.toUpperCase();
  updateReleaseNumberPreview();
});

document.querySelectorAll<HTMLInputElement>('input[name="format"]').forEach((el) => {
  el.addEventListener('change', () => {
    saveFormat();
    updateReleaseNumberPreview();
  });
});

// The archive format is a per-group habit, remembered like the Aminet fields.
const FORMAT_STORAGE_KEY = 'archive-format';

function loadFormats(): Record<string, string> {
  try {
    const parsed: unknown = JSON.parse(localStorage.getItem(FORMAT_STORAGE_KEY) ?? '{}');
    return parsed && typeof parsed === 'object' ? (parsed as Record<string, string>) : {};
  } catch {
    return {};
  }
}

function saveFormat(): void {
  if (!currentProfile) return;
  try {
    localStorage.setItem(
      FORMAT_STORAGE_KEY,
      JSON.stringify({ ...loadFormats(), [currentProfile.id]: getSelectedFormat() }),
    );
  } catch {
    // Storage full or disabled: the choice still works for this session.
  }
}

function restoreFormat(profileId: string): void {
  const saved = loadFormats()[profileId];
  const radio = document.querySelector<HTMLInputElement>(`input[name="format"][value="${saved}"]`);
  if (radio) radio.checked = true;
}

let dryRunRequested = false;
previewBtn.addEventListener('click', () => {
  dryRunRequested = true;
  form.requestSubmit();
  // A failed validation never reaches the submit handler, which clears this.
  dryRunRequested = false;
});

let codeCheckTimer: number | undefined;
codeInput.addEventListener('input', () => checkCode());
/** Shows whether the server already holds the typed code. Re-run after a revoke frees one. */
function checkCode(): void {
  window.clearTimeout(codeCheckTimer);
  const code = codeInput.value.trim();
  codeStatus.textContent = '';
  codeStatus.className = 'status';
  if (!currentProfile || code.length !== currentProfile.codeLength) return;
  const profile = currentProfile;
  codeCheckTimer = window.setTimeout(async () => {
    const { exists } = await window.api.checkCodeAvailability(profile.id, code);
    if (exists) {
      codeStatus.textContent = `${releaseBaseName(profile, code)} already exists — building will fail unless you change the code.`;
      codeStatus.className = 'status warn';
    }
  }, 300);
}

async function addPickedPaths(folders: boolean): Promise<void> {
  const paths = await window.api.pickPaths(folders);
  for (const p of paths) {
    if (!selectedPaths.includes(p)) selectedPaths.push(p);
  }
  renderSelectedFiles();
}
pickFilesBtn.addEventListener('click', () => void addPickedPaths(false));
$<HTMLButtonElement>('pick-folders-btn').addEventListener('click', () => void addPickedPaths(true));

/**
 * The NFO pane (and a placeholder profile's DIZ) shows what a build would
 * write from the form as it is now: main renders it, so the preview and the
 * archive cannot drift apart. Template files are read from disk on each call.
 */
let templatePreviewTimer: ReturnType<typeof setTimeout> | undefined;
let templatePreviewRequest = 0;

function scheduleTemplatePreview(): void {
  clearTimeout(templatePreviewTimer);
  templatePreviewTimer = setTimeout(() => void refreshTemplatePreview(), 400);
}

function showPreviewNotes(notes: string[], warn: boolean): void {
  const text = notes.join('\n');
  nfoPreviewStatus.textContent = text;
  nfoPreviewStatus.hidden = !text;
  nfoPreviewStatus.className = warn ? 'status warn' : 'status';
  if (visibility.dizPreviewOnly) {
    templateStatus.textContent = text;
    templateStatus.className = warn ? 'status warn' : 'status';
  }
}

async function refreshTemplatePreview(): Promise<void> {
  if (!currentProfile) return;
  const request = ++templatePreviewRequest;
  const code = codeInput.value.trim().toUpperCase() || 'X'.repeat(currentProfile.codeLength ?? 5);
  try {
    const preview = await window.api.previewOutputs(readForm(code));
    // A slower answer to an older request must not overwrite a newer one.
    if (request !== templatePreviewRequest || !preview) return;
    nfoPreview.textContent = preview.nfoText ?? '';
    if (visibility.dizPreviewOnly) dizBannerPreview.textContent = preview.dizText;
    const notes = [...preview.problems];
    if (!preview.hasNfoTemplate) notes.push('No release.nfo template in this profile, so the archive gets no NFO.');
    showPreviewNotes(notes, preview.problems.length > 0);
  } catch (err) {
    if (request !== templatePreviewRequest) return;
    showPreviewNotes([err instanceof Error ? err.message : String(err)], true);
  }
}

form.addEventListener('input', scheduleTemplatePreview);
form.addEventListener('change', scheduleTemplatePreview);
// Table edits and page switches do not always raise input events.
form.addEventListener('click', scheduleTemplatePreview);

/**
 * The form as it would be sent to main. The build and the template preview
 * both read it here, so the preview shows exactly what a build would write.
 */
function readForm(code: string): ReleaseFormData {
  const credits = creditsTable.getCredits();
  // A hidden section sends nothing, whatever rows it held before the
  // group was switched.
  const mediaEmbeds = visibility.mediaEmbeds ? mediaEmbedsTable.getMediaEmbeds() : [];

  const boards = boardsTable.getBoards();
  return {
    profileId: profileSelect.value,
    releaseDate: releaseDateInput.value.trim(),
    ...(currentDivision === 'xinnovations'
      ? {
          coder: creditNamesForRole(credits, CODER_ROLES),
          system: xSystemInput.value.trim(),
          design: creditNamesForRole(credits, DESIGN_ROLES),
          // An empty table means "leave the stored list alone", so it is sent
          // as absent rather than as an empty list that would wipe it.
          ...(boards.length > 0 ? { boards } : {}),
        }
      : {}),
    ...(visibility.releaseNumberText ? { releaseNumberText: releaseNumberTextInput.value.trim() } : {}),
    code,
    format: getSelectedFormat(),
    releaseName: releaseNameInput.value.trim(),
    platformTag: platformTagText(),
    releaseNotesText: releaseNotesTextArea.value,
    newsText: newsTextArea.value,
    dizBannerText: currentDizBannerText(),
    members: memberlist.getMembers(),
    entryTitle: visibility.historyEntry ? releaseNameText(releaseNameInput.value) : '',
    entryCategory: visibility.historyEntry ? entryCategoryInput.value.trim() : '',
    entryCreators: creatorsFromCredits(credits),
    selectedPaths,
    description: descriptionInput.value.trim(),
    credits,
    mediaEmbeds,
  };
}

form.addEventListener('submit', async (e) => {
  e.preventDefault();
  const dryRun = dryRunRequested;

  // A new build attempt starts here, whether it's for a brand-new code or a
  // retry — any Publish state left over from a previous build must not
  // linger (see resetPublishState's doc comment).
  resetPublishState();

  const code = codeInput.value.trim().toUpperCase();
  const codeLength = currentProfile?.codeLength ?? 5;
  // Every problem at once, each under its own field, the way a form should.
  const errors: FieldError[] = nativeFieldErrors(form);
  if (code && !new RegExp(`^[A-Z0-9]{${codeLength}}$`).test(code)) {
    errors.push({ field: codeInput, message: `Code must be exactly ${codeLength} letters or digits.` });
  }
  if (selectedPaths.length === 0) {
    errors.push({ field: pickFilesBtn, message: 'Choose at least one file or folder to include.' });
  }
  const mediaEmbedsError = visibility.mediaEmbeds ? mediaEmbedsTable.getValidationError() : null;
  if (mediaEmbedsError) errors.push({ field: mediaEmbedsContainer, message: mediaEmbedsError });
  // The credits are the only place the release's people are entered, so an
  // empty table means the NFO history entry would have a blank creator line
  // and the server would reject the build.
  const creditsProblem = creditsError(creditsTable.getCredits(), visibility.historyEntry);
  if (creditsProblem) errors.push({ field: creditsTbody, message: creditsProblem });
  // Errors are listed in page order so the first one scrolled to is the topmost.
  errors.sort((a, b) => (a.field.compareDocumentPosition(b.field) & Node.DOCUMENT_POSITION_FOLLOWING ? -1 : 1));
  if (!showFieldErrors(form, errors)) {
    buildStatus.textContent = `Not built - ${errors.length === 1 ? '1 field needs' : `${errors.length} fields need`} fixing (marked in red above).`;
    buildStatus.className = 'status warn';
    return;
  }

  const formData = readForm(code);
  const embedImagePaths = visibility.mediaEmbeds ? mediaEmbedsTable.getEmbedImagePaths() : [];

  buildBtn.disabled = true;
  previewBtn.disabled = true;
  buildStatus.textContent = dryRun ? 'Building (dry run)...' : 'Building...';
  buildStatus.className = 'status';
  buildProgressStage.textContent = '';
  buildProgress.hidden = false;
  const stopProgress = window.api.onBuildProgress((p) => {
    buildProgressStage.textContent = `${p.stage}...`;
  });
  try {
    const result = await window.api.buildRelease(formData, dryRun);
    if (result.isDryRun) {
      buildStatus.textContent = visibility.publishWebsite
        ? `Dry run built: ${result.archivePath} (preview release ${result.releaseNumber} — ` +
          'nothing was sent to the server, no release number was consumed).'
        : `Dry run built: ${result.archivePath}`;
      buildStatus.className = 'status ok';
      toast('ok', 'Dry run built.');
      // No real release row exists server-side for a dry run — Publish and
      // the cleanup/delete section both stay hidden (resetPublishState()
      // above already did this; nothing further to show here).
    } else if (!visibility.publishWebsite) {
      // No release server: the archive on disk is the whole release, and
      // Aminet is the only place left to send it.
      buildStatus.textContent = `Built: ${result.archivePath}`;
      buildStatus.className = 'status ok';
      toast('ok', 'Release built.');
      built = { profileId: formData.profileId, code, archivePath: result.archivePath, embedImagePaths: [] };
      aminetSection.hidden = false;
      aminetUploadBtn.disabled = false;
      prefillAminetShort();
      refreshAminetPreview();
    } else {
      buildStatus.textContent =
        `Built release ${result.releaseNumber}: ${result.archivePath} — it is a draft until you ` +
        'publish it below, so nothing about it is public yet. Abandoning it costs nothing; use ' +
        'Revoke to hand the release number straight back.' +
        // The door repository is a separate service with its own queue, so
        // its answer is shown as it worded it - including a refusal, which
        // does not make the build any less finished.
        (result.doorSubmission
          ? ` Door repository: ${result.doorSubmission.message}${
              result.doorSubmission.id ? ` (submission ${result.doorSubmission.id})` : ''
            }`
          : '');
      buildStatus.className = 'status ok';
      toast('ok', `Release ${result.releaseNumber} built as a draft. Publish it below.`);
      built = { profileId: formData.profileId, code, archivePath: result.archivePath, embedImagePaths };
      publishSection.hidden = false;
      // Enabled even without a cover image: a click says what is missing.
      publishBtn.disabled = false;
      // Shown now so the readme can be filled in while the cover image is
      // picked; its Upload button waits for the website publish.
      aminetSection.hidden = false;
      aminetStatus.textContent = 'Upload to Aminet unlocks once the website publish has succeeded.';
      prefillAminetShort();
      refreshAminetPreview();
      // Pre-fill the revoke box with what was just built: that is the release
      // a mistake is most likely to be about, and it is the only one the
      // server will accept anyway until something newer exists.
      revokeCodeInput.value = code;
    }
  } catch (err) {
    buildStatus.textContent = `Build failed: ${err instanceof Error ? err.message : String(err)}`;
    buildStatus.className = 'status warn';
    toast('error', buildStatus.textContent);
  } finally {
    stopProgress();
    buildProgress.hidden = true;
    buildBtn.disabled = false;
    previewBtn.disabled = false;
  }
});

pickImageBtn.addEventListener('click', async () => {
  const picked = await window.api.pickImage();
  if (picked) {
    selectedImagePath = picked;
    selectedImageDiv.textContent = picked;
    const thumb = createThumb(picked);
    if (thumb) selectedImageDiv.prepend(thumb);
    clearFieldErrors(publishSection);
  }
});

publishBtn.addEventListener('click', async () => {
  const release = builtReleaseFor(built, profileSelect.value);
  if (!release) {
    publishStatus.textContent = 'Not published - build the release for this group first.';
    publishStatus.className = 'status warn';
    return;
  }
  const imageOk = showFieldErrors(
    publishSection,
    selectedImagePath ? [] : [{ field: pickImageBtn, message: 'Choose a cover image for the WordPress post.' }],
  );
  // With the Aminet box ticked, an incomplete readme stops the whole Publish
  // before anything is sent: the two are one action to the user, and the
  // website half cannot be redone to pick up the other half.
  const aminetOk = aminetCheckbox.checked ? validateAminetFields() : (clearFieldErrors(aminetSection), true);
  if (!imageOk || !aminetOk || !selectedImagePath) {
    publishStatus.textContent = imageOk
      ? 'Not published - fix the Aminet fields marked below, or untick "Also upload to Aminet".'
      : 'Not published - fix the fields marked in red.';
    publishStatus.className = 'status warn';
    return;
  }
  publishBtn.disabled = true;
  publishStatus.textContent = 'Publishing... (this can take a while for a large archive, please wait)';
  publishStatus.className = 'status';
  try {
    const result = await window.api.publishToWeb(
      release.profileId,
      release.code,
      release.archivePath,
      selectedImagePath,
      release.embedImagePaths,
    );
    if (result.ok) {
      publishStatus.textContent = `Published: ${result.wordpressPostUrl} (archive: ${result.archiveUrl}).`;
      publishStatus.className = 'status ok';
      toast('ok', 'Published to the group website.');
      // Aminet only after the website has the release, as before; its own
      // status line keeps a failed upload from reading as a failed publish.
      aminetUploadBtn.disabled = false;
      if (aminetCheckbox.checked) await uploadLastBuildToAminet();
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
      toast('error', publishStatus.textContent);
      publishBtn.disabled = false;
    } else if (result.status === 409) {
      publishStatus.textContent = result.wordpressPostUrl
        ? `Already published: ${result.wordpressPostUrl}`
        : `Already published: ${result.message}`;
      publishStatus.className = 'status warn';
      aminetUploadBtn.disabled = false;
      // Already published — leave publishBtn disabled, same as the success
      // path. A genuinely-already-published release doesn't need retrying.
    } else {
      publishStatus.textContent = `Publish failed: ${result.message}`;
      publishStatus.className = 'status warn';
      toast('error', publishStatus.textContent);
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
    toast('error', publishStatus.textContent);
    publishBtn.disabled = false;
  }
});

aminetUploadBtn.addEventListener('click', () => void uploadLastBuildToAminet());

deleteReleaseBtn.addEventListener('click', async () => {
  // Read from the input, not from this session's build: a release that needs
  // revoking is often one from an earlier run of the app, after the mistake
  // has been noticed. Requiring a fresh build to reach the button left those
  // unreachable, which is exactly how two abandoned builds sat in the public
  // catalogue.
  const deletedCode = revokeCodeInput.value.trim().toUpperCase();
  if (!showFieldErrors(revokeCodeInput.parentElement ?? document, deletedCode ? [] : [{ field: revokeCodeInput, message: 'Enter the code of the release to revoke.' }])) {
    deleteStatus.textContent = '';
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
    const result = await window.api.deleteRelease(profileSelect.value, deletedCode);
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
      checkCode();
      toast(deleteStatus.className === 'status ok' ? 'ok' : 'info', `Release ${deletedCode} revoked.`);
      // Revoking the release this session just built leaves nothing to
      // publish, so the publish state goes with it — a stray click on Publish
      // must not fire against a code that is gone. Revoking some older
      // release leaves this session's own build alone. deleteStatus stays as
      // it is either way: it may carry a WordPress-post URL the user still
      // has to act on.
      if (deletedCode === builtReleaseFor(built, profileSelect.value)?.code) {
        built = null;
        publishSection.hidden = true;
        publishStatus.textContent = '';
        publishStatus.className = 'status';
        publishBtn.disabled = true;
        aminetSection.hidden = true;
        aminetUploadBtn.disabled = true;
      }
      deleteReleaseBtn.disabled = false;
    } else if (result.status === 409) {
      deleteStatus.textContent = `Could not revoke: ${result.message} (a newer release exists — this is no longer the latest).`;
      deleteStatus.className = 'status warn';
      toast('error', deleteStatus.textContent);
      deleteReleaseBtn.disabled = false;
    } else {
      deleteStatus.textContent = `Revoke failed: ${result.message}`;
      deleteStatus.className = 'status warn';
      toast('error', deleteStatus.textContent);
      deleteReleaseBtn.disabled = false;
    }
  } catch (err) {
    deleteStatus.textContent = `Revoke failed: ${err instanceof Error ? err.message : String(err)}`;
    deleteStatus.className = 'status warn';
    toast('error', deleteStatus.textContent);
    deleteReleaseBtn.disabled = false;
  }
});

initWizard();
applyFoldDefaults();
void loadProfiles();
