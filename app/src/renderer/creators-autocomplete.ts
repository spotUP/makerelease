import { MemberEntry } from '../shared/types';

function currentTokenBounds(value: string, caret: number): { start: number; end: number } {
  const lastComma = value.lastIndexOf(',', caret - 1);
  const start = lastComma === -1 ? 0 : lastComma + 1;
  return { start, end: caret };
}

export interface CreatorsAutocompleteOptions {
  /**
   * False for a field that holds ONE name - a credit row's Name - rather
   * than a comma-separated list. The whole value is then the search token
   * and a pick replaces it, instead of appending "<name>, " to a list.
   */
  multiple?: boolean;
}

export function attachCreatorsAutocomplete(
  input: HTMLInputElement,
  list: HTMLUListElement,
  getMembers: () => MemberEntry[],
  options: CreatorsAutocompleteOptions = {},
): void {
  const multiple = options.multiple !== false;
  let activeIndex = -1;
  let matches: string[] = [];

  function hide(): void {
    list.hidden = true;
    list.innerHTML = '';
    matches = [];
    activeIndex = -1;
  }

  function render(): void {
    list.innerHTML = '';
    matches.forEach((nickname, i) => {
      const li = document.createElement('li');
      li.textContent = nickname;
      if (i === activeIndex) li.className = 'active';
      li.addEventListener('mousedown', (e) => {
        e.preventDefault();
        select(nickname);
      });
      list.appendChild(li);
    });
    list.hidden = matches.length === 0;
  }

  function select(nickname: string): void {
    if (!multiple) {
      input.value = nickname;
      input.setSelectionRange(nickname.length, nickname.length);
      hide();
      input.focus();
      return;
    }
    const caret = input.selectionStart ?? input.value.length;
    const { start, end } = currentTokenBounds(input.value, caret);
    const before = input.value.slice(0, start).replace(/\s+$/, '');
    const after = input.value.slice(end);
    const prefix = before.length > 0 ? `${before} ` : '';
    const insertion = `${prefix}${nickname}, `;
    input.value = insertion + after;
    const newCaret = insertion.length;
    input.setSelectionRange(newCaret, newCaret);
    hide();
    input.focus();
  }

  function update(): void {
    const caret = input.selectionStart ?? input.value.length;
    const { start, end } = multiple
      ? currentTokenBounds(input.value, caret)
      : { start: 0, end: input.value.length };
    const token = input.value.slice(start, end).trim().toLowerCase();
    if (!token) {
      hide();
      return;
    }
    // Only a list field can already hold the other names; a single-name
    // field's own value IS the token being completed.
    const taken = multiple
      ? new Set(
          input.value
            .split(',')
            .map((s) => s.trim().toLowerCase())
            .filter(Boolean),
        )
      : new Set<string>();
    matches = getMembers()
      .map((m) => m.nickname)
      .filter((nickname) => nickname.toLowerCase().startsWith(token) && !taken.has(nickname.toLowerCase()))
      .slice(0, 8);
    activeIndex = matches.length > 0 ? 0 : -1;
    render();
  }

  input.addEventListener('input', update);
  input.addEventListener('click', update);

  input.addEventListener('keydown', (e) => {
    if (list.hidden || matches.length === 0) return;
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      activeIndex = (activeIndex + 1) % matches.length;
      render();
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      activeIndex = (activeIndex - 1 + matches.length) % matches.length;
      render();
    } else if (e.key === 'Enter' || e.key === 'Tab') {
      if (activeIndex >= 0) {
        e.preventDefault();
        select(matches[activeIndex]);
      }
    } else if (e.key === 'Escape') {
      hide();
    }
  });

  input.addEventListener('blur', () => {
    window.setTimeout(hide, 100);
  });
}
