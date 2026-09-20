import { describe, it, expect } from 'vitest';
import { boardsFromRows } from '../../src/renderer/boards-table';

describe('boardsFromRows', () => {
  it('keeps the boards in the order they were entered', () => {
    expect(
      boardsFromRows([
        ['uPTOWN', 'sPOT', 'wEBHQ', 'SOON'],
        ['tHE yARD', 'zANER', 'gHQ', 'tEMP-dOWN'],
      ]).map((b) => b.name),
    ).toEqual(['uPTOWN', 'tHE yARD']);
  });

  it('ignores a row that names no board', () => {
    expect(boardsFromRows([['', 'nobody', '', '']])).toEqual([]);
    expect(boardsFromRows([['   ', '', '', '']])).toEqual([]);
  });

  it('takes a board that has only a name - a board with no HQ yet is still a board', () => {
    expect(boardsFromRows([['sANCTUARY', '', '', '']])).toEqual([
      { name: 'sANCTUARY', sysop: '', hq: '', status: '' },
    ]);
  });

  it('trims what was typed', () => {
    expect(boardsFromRows([['  uPTOWN  ', ' sPOT ', ' wEBHQ ', ' SOON ']])).toEqual([
      { name: 'uPTOWN', sysop: 'sPOT', hq: 'wEBHQ', status: 'SOON' },
    ]);
  });

  it('is empty for an empty table, which means "leave the stored list alone"', () => {
    expect(boardsFromRows([])).toEqual([]);
  });
});
