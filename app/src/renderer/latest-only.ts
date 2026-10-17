/**
 * Race token for async work started in quick succession (profile switches):
 * only the most recently begun run may apply its result.
 */
export class LatestOnly {
  private latest = 0;

  /** Starts a run; the returned check is true only until the next begin(). */
  begin(): () => boolean {
    const mine = ++this.latest;
    return () => mine === this.latest;
  }
}
