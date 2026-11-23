/* JSON writer over strbuf; string escaping matches JavaScript JSON.stringify
 * (\" \\ \b \f \n \r \t, other C0 as \u00xx lowercase; bytes >= 0x80 pass
 * through untouched, so UTF-8 in means UTF-8 out). Compact, no spaces. */
#ifndef RP_JSON_WRITE_H
#define RP_JSON_WRITE_H

#include "strbuf.h"

/* Each returns 0 or -1 (out of memory). */
int jw_str(strbuf *b, const char *s);                 /* "escaped" */
int jw_key(strbuf *b, const char *key, int first);    /* [,]"key": */

#endif
