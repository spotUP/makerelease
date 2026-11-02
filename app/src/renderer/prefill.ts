/**
 * The release name as text for other fields: the release notes write it in
 * quotes ("'FLEISCH BITTE AD 006'"), which neither the diz line nor the
 * WordPress description wants.
 */
export function releaseNameText(releaseName: string): string {
  return releaseName.trim().replace(/^(['"])(.*)\1$/, '$2').trim();
}

/**
 * A field that copies another until the user types into it.
 *
 * The field counts as untouched while it still shows exactly what was last
 * filled in (or is empty); once it shows anything else, the user's text wins
 * and the field stops following.
 */
export class Prefill {
  private filled: string;

  constructor(initial = '') {
    this.filled = initial;
  }

  /** Starts over, as if `value` had just been filled in. */
  reset(value: string): void {
    this.filled = value;
  }

  /**
   * What the field should show now that the source says `value`, or
   * undefined when the user has made the field their own.
   */
  next(current: string, value: string): string | undefined {
    if (current !== '' && current !== this.filled) return undefined;
    this.filled = value;
    return value;
  }
}
