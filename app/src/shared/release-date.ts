/**
 * Today, as the group writes dates: YYYY-MM-DD, the format the /X NFO's
 * "rELEASE dATE [...]" slot is drawn for.
 *
 * Deliberately local time, not UTC: a release built at 23:30 in Sweden is
 * dated that day, not tomorrow, and toISOString() would say tomorrow.
 */
export function todayIso(now: Date = new Date()): string {
  const pad = (n: number): string => String(n).padStart(2, '0');
  return `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
}
