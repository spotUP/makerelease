export const DIZ_COLS = 45;
export const DIZ_ROWS = 10;
const STRIDE = DIZ_COLS + 1; // +1 for the '\n' between rows

function padLine(line: string): string {
  return line.slice(0, DIZ_COLS).padEnd(DIZ_COLS, ' ');
}

export function toGridText(freeform: string): string {
  const lines = freeform.split('\n').slice(0, DIZ_ROWS).map(padLine);
  while (lines.length < DIZ_ROWS) lines.push(' '.repeat(DIZ_COLS));
  return lines.join('\n');
}

function rowColFromIndex(index: number): { row: number; col: number } {
  const row = Math.min(DIZ_ROWS - 1, Math.floor(index / STRIDE));
  const col = Math.min(DIZ_COLS, index - row * STRIDE);
  return { row, col };
}

function indexFromRowCol(row: number, col: number): number {
  const clampedRow = Math.max(0, Math.min(DIZ_ROWS - 1, row));
  const clampedCol = Math.max(0, Math.min(DIZ_COLS, col));
  return clampedRow * STRIDE + clampedCol;
}

function setChar(grid: string, row: number, col: number, char: string): string {
  const index = row * STRIDE + col;
  return grid.slice(0, index) + char + grid.slice(index + 1);
}

export function attachOverwriteGrid(textarea: HTMLTextAreaElement): void {
  textarea.value = toGridText(textarea.value);

  function setCaret(row: number, col: number): void {
    const index = indexFromRowCol(row, col);
    textarea.selectionStart = index;
    textarea.selectionEnd = index;
  }

  textarea.addEventListener('keydown', (e) => {
    const { row, col } = rowColFromIndex(textarea.selectionStart);

    if (e.key === 'Enter') {
      e.preventDefault();
      setCaret(row + 1, 0);
      return;
    }

    if (e.key === 'Backspace') {
      e.preventDefault();
      if (col > 0) {
        textarea.value = setChar(textarea.value, row, col - 1, ' ');
        setCaret(row, col - 1);
      }
      return;
    }

    if (e.key === 'Delete') {
      e.preventDefault();
      if (col < DIZ_COLS) {
        textarea.value = setChar(textarea.value, row, col, ' ');
        setCaret(row, col);
      }
      return;
    }

    const isPrintable = e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey;
    if (isPrintable) {
      e.preventDefault();
      if (col < DIZ_COLS) {
        textarea.value = setChar(textarea.value, row, col, e.key);
        setCaret(row, col + 1);
      }
      return;
    }
    // Arrow keys, Home/End, Tab, etc. fall through to native caret behavior.
  });

  textarea.addEventListener('paste', (e) => {
    e.preventDefault();
    const pasted = e.clipboardData?.getData('text/plain') ?? '';
    const pastedLines = pasted.split('\n');
    const { row: startRow, col: startCol } = rowColFromIndex(textarea.selectionStart);

    let value = textarea.value;
    let lastRow = startRow;
    let lastCol = startCol;
    for (let i = 0; i < pastedLines.length; i++) {
      const targetRow = startRow + i;
      if (targetRow >= DIZ_ROWS) break;
      const startColumn = i === 0 ? startCol : 0;
      const chars = pastedLines[i];
      let col = startColumn;
      for (const char of chars) {
        if (col >= DIZ_COLS) break;
        value = setChar(value, targetRow, col, char);
        col++;
      }
      lastRow = targetRow;
      lastCol = col;
    }
    textarea.value = value;
    setCaret(lastRow, lastCol);
    textarea.dispatchEvent(new Event('input'));
  });
}
