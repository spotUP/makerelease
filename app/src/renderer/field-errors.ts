/**
 * Form-style validation messages: each problem is printed in red directly
 * under the input (or table, or list) that fixes it, the input is marked
 * invalid for screen readers, and the first one is scrolled to and focused.
 * A message goes away as soon as its field is edited.
 */
export interface FieldError {
  field: HTMLElement;
  message: string;
}

let revealHandler: ((el: HTMLElement) => void) | null = null;

/** Registers the hook that shows the page holding the first error (the wizard). */
export function setRevealHandler(fn: (el: HTMLElement) => void): void {
  revealHandler = fn;
}

const ERROR_CLASS = 'field-error';
let nextId = 0;

/** The field's label text without its parenthesised hint, for "X is required." */
export function fieldLabel(field: HTMLElement): string {
  const label = (field.id && document.querySelector<HTMLLabelElement>(`label[for="${field.id}"]`)) || field.closest('label');
  const text = label?.childNodes[0]?.textContent ?? label?.textContent ?? '';
  return text.replace(/\s*[(—].*$/s, '').trim() || 'This field';
}

/** Removes one field's message, for fields that fire no input event (buttons). */
export function clearFieldError(field: HTMLElement): void {
  field.removeAttribute('aria-invalid');
  const id = field.dataset.fieldErrorId;
  if (id) document.getElementById(id)?.remove();
  delete field.dataset.fieldErrorId;
  field.removeAttribute('aria-describedby');
}

/** Removes every message shown inside `root`. */
export function clearFieldErrors(root: ParentNode): void {
  root.querySelectorAll<HTMLElement>('[data-field-error-id]').forEach(clearFieldError);
}

/**
 * Replaces the messages inside `root` with `errors`. Several messages for one
 * field are joined. Returns true when there was nothing to show.
 */
export function showFieldErrors(root: ParentNode, errors: FieldError[]): boolean {
  clearFieldErrors(root);
  const byField = new Map<HTMLElement, string[]>();
  for (const { field, message } of errors) byField.set(field, [...(byField.get(field) ?? []), message]);

  for (const [field, messages] of byField) {
    const note = document.createElement('div');
    note.className = ERROR_CLASS;
    note.id = `field-error-${nextId++}`;
    note.setAttribute('role', 'alert');
    note.textContent = messages.join(' ');
    // A checkbox or input inside a label gets its message after the label.
    (field.closest('label') ?? field).after(note);
    field.dataset.fieldErrorId = note.id;
    field.setAttribute('aria-invalid', 'true');
    field.setAttribute('aria-describedby', note.id);
    const clear = (): void => clearFieldError(field);
    field.addEventListener('input', clear, { once: true });
    field.addEventListener('change', clear, { once: true });
  }

  const first = errors[0]?.field;
  if (first) {
    // Fields inside a closed fold, or on another wizard page, must be shown first.
    for (const { field } of errors) {
      for (let d = field.closest('details'); d; d = d.parentElement?.closest('details') ?? null) d.open = true;
    }
    revealHandler?.(first);
    first.scrollIntoView({ block: 'center', behavior: 'smooth' });
    first.focus({ preventScroll: true });
  }
  return errors.length === 0;
}

/** Native constraint failures (required, maxlength, type=email) inside a form, as messages. */
export function nativeFieldErrors(form: HTMLFormElement): FieldError[] {
  return Array.from(form.elements)
    .filter((el): el is HTMLInputElement | HTMLSelectElement | HTMLTextAreaElement =>
      el instanceof HTMLInputElement || el instanceof HTMLSelectElement || el instanceof HTMLTextAreaElement,
    )
    .filter((el) => !el.closest('[hidden]') && !el.checkValidity())
    .map((el) => ({
      field: el,
      message: el.validity.valueMissing ? `${fieldLabel(el)} is required.` : el.validationMessage,
    }));
}
