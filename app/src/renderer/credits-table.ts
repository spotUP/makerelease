import { CreditEntry, MemberEntry } from '../shared/types';
import { attachCreatorsAutocomplete } from './creators-autocomplete.js';

export interface CreditsTableOptions {
  /**
   * Roster to complete the Name field against. The credit rows are now the
   * only place a release's people are typed (the NFO creator line is derived
   * from them - see credits-creators.ts), so the member autocomplete that
   * used to sit on the Creator(s) field belongs here.
   */
  getMembers?: () => MemberEntry[];
  /** Fired when a row is added or removed, so the derived creator line can be re-rendered. */
  onChange?: () => void;
}

// Deliberately NOT sorted (unlike MemberlistTable's alphabetical roster
// sort) - credits render in the order given ("Code, Music, Animation..."),
// and that order is meaningful to preserve.
export class CreditsTable {
  private tbody: HTMLTableSectionElement;
  private options: CreditsTableOptions;

  constructor(tbody: HTMLTableSectionElement, options: CreditsTableOptions = {}) {
    this.tbody = tbody;
    this.options = options;
  }

  addRow(credit: CreditEntry = { role: '', name: '' }): void {
    const row = document.createElement('tr');

    const roleCell = document.createElement('td');
    const roleInput = document.createElement('input');
    roleInput.type = 'text';
    roleInput.placeholder = 'Code';
    roleInput.value = credit.role;
    roleCell.appendChild(roleInput);

    const nameCell = document.createElement('td');
    const nameWrap = document.createElement('div');
    nameWrap.className = 'autocomplete-wrap';
    const nameInput = document.createElement('input');
    nameInput.type = 'text';
    nameInput.placeholder = 'FluBBa';
    nameInput.autocomplete = 'off';
    nameInput.value = credit.name;
    const nameSuggestions = document.createElement('ul');
    nameSuggestions.className = 'autocomplete-list';
    nameSuggestions.hidden = true;
    nameWrap.append(nameInput, nameSuggestions);
    nameCell.appendChild(nameWrap);

    const { getMembers } = this.options;
    if (getMembers) {
      attachCreatorsAutocomplete(nameInput, nameSuggestions, getMembers, { multiple: false });
    }

    const actionCell = document.createElement('td');
    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.textContent = 'Remove';
    removeBtn.className = 'danger';
    removeBtn.addEventListener('click', () => {
      row.remove();
      this.options.onChange?.();
    });
    actionCell.appendChild(removeBtn);

    row.append(roleCell, nameCell, actionCell);
    this.tbody.appendChild(row);
    this.options.onChange?.();
  }

  getCredits(): CreditEntry[] {
    const credits: CreditEntry[] = [];
    for (const row of Array.from(this.tbody.rows)) {
      const inputs = row.querySelectorAll('input');
      const role = (inputs[0] as HTMLInputElement).value.trim();
      const name = (inputs[1] as HTMLInputElement).value.trim();
      if (role && name) credits.push({ role, name });
    }
    return credits;
  }
}
