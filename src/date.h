#ifndef RP_DATE_H
#define RP_DATE_H

#include <stddef.h>

/* days_since_1970 -> "YYYY-MM-DD" (civil calendar, Howard Hinnant's
 * algorithm). Amiga dos.library counts days since 1978-01-01: add 2922. */
void rp_iso_date(long days_since_1970, char *out, size_t cap);

#endif
