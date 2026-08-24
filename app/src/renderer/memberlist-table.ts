import { MemberEntry } from '../shared/types';

export class MemberlistTable {
  private tbody: HTMLTableSectionElement;

  constructor(tbody: HTMLTableSectionElement) {
    this.tbody = tbody;
  }

  seed(members: MemberEntry[]): void {
    this.tbody.innerHTML = '';
    for (const member of members) this.addRow(member);
  }

  addRow(member: MemberEntry = { nickname: '', role: '' }): void {
    const row = document.createElement('tr');

    const nicknameCell = document.createElement('td');
    const nicknameInput = document.createElement('input');
    nicknameInput.type = 'text';
    nicknameInput.value = member.nickname;
    nicknameCell.appendChild(nicknameInput);

    const roleCell = document.createElement('td');
    const roleInput = document.createElement('input');
    roleInput.type = 'text';
    roleInput.value = member.role;
    roleCell.appendChild(roleInput);

    const actionCell = document.createElement('td');
    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.textContent = 'Remove';
    removeBtn.className = 'danger';
    removeBtn.addEventListener('click', () => row.remove());
    actionCell.appendChild(removeBtn);

    row.append(nicknameCell, roleCell, actionCell);
    this.tbody.appendChild(row);
  }

  getMembers(): MemberEntry[] {
    const members: MemberEntry[] = [];
    for (const row of Array.from(this.tbody.rows)) {
      const inputs = row.querySelectorAll('input');
      const nickname = (inputs[0] as HTMLInputElement).value.trim();
      const role = (inputs[1] as HTMLInputElement).value.trim();
      if (nickname) members.push({ nickname, role });
    }
    return members.sort((a, b) => a.nickname.localeCompare(b.nickname));
  }
}
