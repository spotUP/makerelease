import { describe, it, expect } from 'vitest';
import * as fs from 'fs';
import { GOLD_DIZ_PATH, GOLD_NFO_PATH, renderGold } from './ur-golden.fixture';

// Parity proof for the Up Rough code renderer: the fixtures were written
// from the code before profiles existed, and every later change must leave
// the bytes alone.
describe('the Up Rough NFO and DIZ against the frozen golden files', () => {
  const { nfoText, dizText } = renderGold();

  it('renders the NFO byte for byte', () => {
    expect(nfoText).toBe(fs.readFileSync(GOLD_NFO_PATH, 'latin1'));
  });

  it('renders the DIZ byte for byte', () => {
    expect(dizText).toBe(fs.readFileSync(GOLD_DIZ_PATH, 'latin1'));
  });
});
