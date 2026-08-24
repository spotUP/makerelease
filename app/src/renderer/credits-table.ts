import { CreditEntry } from '../shared/types';

// Deliberately NOT sorted (unlike MemberlistTable's alphabetical roster
// sort) - credits render in the order given ("Code, Music, Animation..."),
// and that order is meaningful to preserve.
export class CreditsTable {
  private tbody: HTMLTableSectionElement;

  constructor(tbody: HTMLTableSectionElement) {
    this.tbody = tbody;
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
    const nameInput = document.createElement('input');
    nameInput.type = 'text';
    nameInput.placeholder = 'FluBBa';
    nameInput.value = credit.name;
    nameCell.appendChild(nameInput);

    const actionCell = document.createElement('td');
    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.textContent = 'Remove';
    removeBtn.className = 'danger';
    removeBtn.addEventListener('click', () => row.remove());
    actionCell.appendChild(removeBtn);

    row.append(roleCell, nameCell, actionCell);
    this.tbody.appendChild(row);
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
