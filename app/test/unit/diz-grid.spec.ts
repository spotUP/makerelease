import { describe, expect, it } from 'vitest';
import { toGridText, DIZ_ROWS, DIZ_COLS } from '../../src/renderer/diz-grid';

describe('toGridText', () => {
  it('pads short content to exactly 10 lines of 45 chars', () => {
    const grid = toGridText('hello');
    const lines = grid.split('\n');
    expect(lines.length).toBe(DIZ_ROWS);
    expect(lines.every((l) => l.length === DIZ_COLS)).toBe(true);
    expect(lines[0]).toBe('hello'.padEnd(DIZ_COLS, ' '));
  });

  it('truncates content beyond 10 lines or 45 columns', () => {
    const oversized = Array.from({ length: 14 }, (_, i) => `line ${i}`.padEnd(50, 'x')).join('\n');
    const grid = toGridText(oversized);
    const lines = grid.split('\n');
    expect(lines.length).toBe(DIZ_ROWS);
    expect(lines.every((l) => l.length === DIZ_COLS)).toBe(true);
  });

  it('is idempotent on already-gridded text', () => {
    const grid = toGridText('a\nb');
    expect(toGridText(grid)).toBe(grid);
  });
});
