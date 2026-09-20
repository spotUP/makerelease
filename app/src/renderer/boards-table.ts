import { BoardEntry } from '../shared/types';

/**
 * The rule for turning typed rows into a board list, kept out of the DOM so
 * it can be tested: a row counts if it names a board, and the other three
 * columns are optional - a board with no HQ yet is still a board.
 */
export function boardsFromRows(rows: ReadonlyArray<readonly string[]>): BoardEntry[] {
  const boards: BoardEntry[] = [];
  for (const row of rows) {
    const value = (i: number): string => (row[i] ?? '').trim();
    const name = value(0);
    if (!name) continue;
    boards.push({ name, sysop: value(1), hq: value(2), status: value(3) });
  }
  return boards;
}

/**
 * The board list the /X NFO prints. Like the credits table, rows keep the
 * order they were entered - the NFO prints them in that order, and it is not
 * alphabetical (the group's own board comes first).
 *
 * An empty table is not an empty list: a release that sends no boards leaves
 * the stored list alone, so the last release's boards carry forward.
 */
export class BoardsTable {
  private tbody: HTMLTableSectionElement;

  constructor(tbody: HTMLTableSectionElement) {
    this.tbody = tbody;
  }

  addRow(board: BoardEntry = { name: '', sysop: '', hq: '', status: '' }): void {
    const row = document.createElement('tr');
    const fields: Array<[keyof BoardEntry, string]> = [
      ['name', 'uPTOWN'],
      ['sysop', 'sPOT'],
      ['hq', 'wEBHQ'],
      ['status', 'SOON'],
    ];
    for (const [field, placeholder] of fields) {
      const cell = document.createElement('td');
      const input = document.createElement('input');
      input.type = 'text';
      input.autocomplete = 'off';
      input.placeholder = placeholder;
      input.value = board[field];
      cell.appendChild(input);
      row.appendChild(cell);
    }

    const actionCell = document.createElement('td');
    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.textContent = 'Remove';
    removeBtn.className = 'danger';
    removeBtn.addEventListener('click', () => row.remove());
    actionCell.appendChild(removeBtn);
    row.appendChild(actionCell);

    this.tbody.appendChild(row);
  }

  seed(boards: BoardEntry[]): void {
    this.tbody.innerHTML = '';
    for (const board of boards) this.addRow(board);
  }

  /** Rows with a name; a row needs nothing else to count. */
  getBoards(): BoardEntry[] {
    return boardsFromRows(
      Array.from(this.tbody.rows).map((row) =>
        Array.from(row.querySelectorAll('input')).map((input) => (input as HTMLInputElement).value),
      ),
    );
  }
}
