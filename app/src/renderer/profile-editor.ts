import type { BackendKind, RendererKind } from '../shared/profile.js';
import { slugifyProfileId } from '../shared/profile.js';
import type { ProfileEditRequest, ProfileEditState, ProfileTemplatesText } from '../shared/types';

export type ProfileEditorMode = 'new' | 'edit' | 'duplicate';

/** The dialog's fields as typed; formToRequest turns them into what main validates and writes. */
export interface ProfileForm {
  id: string;
  isNew: boolean;
  label: string;
  url: string;
  filePrefix: string;
  codeLength: number;
  renderer: RendererKind;
  backend: BackendKind;
  baseUrl: string;
  writeKey: string;
  members: Array<{ nickname: string; role: string }>;
  templates: ProfileTemplatesText;
}

/** Fills the form from a loaded profile; Duplicate keeps every field, with a fresh id and " copy" on the name. */
export function profileToForm(state: ProfileEditState, mode: ProfileEditorMode, takenIds: readonly string[]): ProfileForm {
  const { profile, templates } = state;
  const label = mode === 'duplicate' ? `${profile.label} copy` : profile.label;
  return {
    id: mode === 'duplicate' ? slugifyProfileId(label, takenIds) : profile.id,
    isNew: mode !== 'edit',
    label,
    url: profile.url,
    filePrefix: profile.filePrefix,
    codeLength: profile.codeLength,
    renderer: profile.renderer,
    backend: profile.backend,
    baseUrl: profile.server?.baseUrl ?? '',
    writeKey: profile.server?.writeKey ?? '',
    members: profile.members.map((member) => ({ ...member })),
    templates: { ...templates },
  };
}

/**
 * Typing only: no rule of profile.json is checked here. Main runs the
 * result through parseProfile and sends back its message on refusal.
 */
export function formToRequest(form: ProfileForm): ProfileEditRequest {
  const doorserver = form.backend === 'releases-doorserver';
  return {
    id: form.id,
    isNew: form.isNew,
    profile: {
      label: form.label,
      url: form.url,
      filePrefix: form.filePrefix,
      codeLength: form.codeLength,
      renderer: form.renderer,
      backend: form.backend,
      ...(doorserver ? { server: { baseUrl: form.baseUrl, writeKey: form.writeKey } } : {}),
      members: form.members.filter((member) => member.nickname.trim() !== ''),
    },
    ...(form.renderer === 'placeholder' ? { templates: form.templates } : {}),
  };
}

const $ = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;

export interface ProfileEditor {
  /** Resolves with the saved id, or null when the dialog was cancelled. */
  open(mode: ProfileEditorMode, selectedId: string | null, takenIds: readonly string[]): Promise<string | null>;
}

export function createProfileEditor(): ProfileEditor {
  const dialog = $<HTMLDialogElement>('profile-dialog');
  const title = $<HTMLElement>('profile-dialog-title');
  const labelInput = $<HTMLInputElement>('pe-label');
  const idInput = $<HTMLInputElement>('pe-id');
  const urlInput = $<HTMLInputElement>('pe-url');
  const prefixInput = $<HTMLInputElement>('pe-prefix');
  const codeLengthInput = $<HTMLInputElement>('pe-code-length');
  const rendererSelect = $<HTMLSelectElement>('pe-renderer');
  const backendSelect = $<HTMLSelectElement>('pe-backend');
  const serverBox = $<HTMLElement>('pe-server');
  const baseUrlInput = $<HTMLInputElement>('pe-base-url');
  const writeKeyInput = $<HTMLInputElement>('pe-write-key');
  const showKeyBtn = $<HTMLButtonElement>('pe-show-key');
  const membersBody = $<HTMLTableSectionElement>('pe-members');
  const membersNote = $<HTMLElement>('pe-members-note');
  const templatesBox = $<HTMLElement>('pe-templates');
  const dizInfo = $<HTMLElement>('pe-diz-info');
  const nfoInfo = $<HTMLElement>('pe-nfo-info');
  const errorLine = $<HTMLElement>('pe-error');

  let form: ProfileForm | null = null;
  let takenIds: readonly string[] = [];
  let idTouched = false;
  let finish: ((id: string | null) => void) | null = null;

  const showError = (message: string): void => {
    errorLine.textContent = message;
  };

  function renderMembers(): void {
    if (!form) return;
    const members = form.members;
    membersBody.replaceChildren(
      ...members.map((member, index) => {
        const row = document.createElement('tr');
        const cell = (child: HTMLElement): HTMLTableCellElement => {
          const td = document.createElement('td');
          td.append(child);
          return td;
        };
        const field = (value: string, assign: (v: string) => void): HTMLInputElement => {
          const input = document.createElement('input');
          input.type = 'text';
          input.value = value;
          input.autocomplete = 'off';
          input.addEventListener('input', () => assign(input.value));
          return input;
        };
        const button = (text: string, action: () => void): HTMLButtonElement => {
          const b = document.createElement('button');
          b.type = 'button';
          b.textContent = text;
          b.addEventListener('click', action);
          return b;
        };
        const move = (by: number): void => {
          const target = index + by;
          if (target < 0 || target >= members.length) return;
          [members[index], members[target]] = [members[target], members[index]];
          renderMembers();
        };
        const actions = document.createElement('td');
        actions.append(
          button('Up', () => move(-1)),
          button('Down', () => move(1)),
          button('Remove', () => {
            members.splice(index, 1);
            renderMembers();
          }),
        );
        row.append(
          cell(field(member.nickname, (v) => (member.nickname = v))),
          cell(field(member.role, (v) => (member.role = v))),
          actions,
        );
        return row;
      }),
    );
  }

  function renderTemplateInfo(): void {
    if (!form) return;
    dizInfo.textContent = form.templates.diz === '' ? 'empty' : `${form.templates.diz.split('\n').length - 1} lines`;
    nfoInfo.textContent =
      form.templates.nfo === null ? 'none, the archive gets no NFO' : `${form.templates.nfo.split('\n').length - 1} lines`;
  }

  function applyVisibility(): void {
    serverBox.hidden = backendSelect.value !== 'releases-doorserver';
    membersNote.hidden = backendSelect.value !== 'releases-doorserver';
    templatesBox.hidden = rendererSelect.value !== 'placeholder';
  }

  function readFields(): ProfileForm {
    if (!form) throw new Error('The group editor is not open');
    return {
      ...form,
      id: idInput.value,
      label: labelInput.value,
      url: urlInput.value,
      filePrefix: prefixInput.value,
      codeLength: codeLengthInput.valueAsNumber,
      renderer: rendererSelect.value as RendererKind,
      backend: backendSelect.value as BackendKind,
      baseUrl: baseUrlInput.value,
      writeKey: writeKeyInput.value,
    };
  }

  function close(id: string | null): void {
    dialog.close();
    finish?.(id);
    finish = null;
  }

  async function importInto(target: 'diz' | 'nfo'): Promise<void> {
    if (!form) return;
    try {
      const picked = await window.api.importTemplate();
      if (!picked) return;
      form.templates[target] = picked.text;
      renderTemplateInfo();
    } catch (err) {
      showError(err instanceof Error ? err.message : String(err));
    }
  }

  labelInput.addEventListener('input', () => {
    if (form?.isNew && !idTouched) idInput.value = slugifyProfileId(labelInput.value, takenIds);
  });
  idInput.addEventListener('input', () => {
    idTouched = true;
  });
  rendererSelect.addEventListener('change', applyVisibility);
  backendSelect.addEventListener('change', applyVisibility);
  showKeyBtn.addEventListener('click', () => {
    const hidden = writeKeyInput.type === 'password';
    writeKeyInput.type = hidden ? 'text' : 'password';
    showKeyBtn.textContent = hidden ? 'Hide' : 'Show';
  });
  $<HTMLButtonElement>('pe-add-member').addEventListener('click', () => {
    form?.members.push({ nickname: '', role: '' });
    renderMembers();
  });
  $<HTMLButtonElement>('pe-import-diz').addEventListener('click', () => void importInto('diz'));
  $<HTMLButtonElement>('pe-import-nfo').addEventListener('click', () => void importInto('nfo'));
  $<HTMLButtonElement>('pe-remove-nfo').addEventListener('click', () => {
    if (form) form.templates.nfo = null;
    renderTemplateInfo();
  });
  $<HTMLButtonElement>('pe-cancel').addEventListener('click', () => close(null));
  dialog.addEventListener('cancel', () => {
    finish?.(null);
    finish = null;
  });
  $<HTMLButtonElement>('pe-save').addEventListener('click', async () => {
    showError('');
    const current = readFields();
    form = current;
    try {
      const outcome = await window.api.saveProfile(formToRequest(current));
      if (outcome.ok) close(current.id);
      else showError(outcome.error);
    } catch (err) {
      showError(err instanceof Error ? err.message : String(err));
    }
  });

  return {
    async open(mode, selectedId, taken) {
      showError('');
      takenIds = taken;
      idTouched = false;
      const state =
        mode === 'new' || selectedId === null
          ? await window.api.newProfileDraft()
          : await window.api.loadProfileForEdit(selectedId);
      form = profileToForm(state, mode, taken);
      title.textContent = { new: 'New group', edit: 'Edit group', duplicate: 'Duplicate group' }[mode];
      labelInput.value = form.label;
      idInput.value = form.id;
      idInput.readOnly = mode === 'edit';
      urlInput.value = form.url;
      prefixInput.value = form.filePrefix;
      codeLengthInput.valueAsNumber = form.codeLength;
      rendererSelect.value = form.renderer;
      backendSelect.value = form.backend;
      baseUrlInput.value = form.baseUrl;
      writeKeyInput.value = form.writeKey;
      writeKeyInput.type = 'password';
      showKeyBtn.textContent = 'Show';
      renderMembers();
      renderTemplateInfo();
      applyVisibility();
      dialog.showModal();
      return new Promise<string | null>((resolve) => {
        finish = resolve;
      });
    },
  };
}
