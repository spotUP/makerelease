/* Minimal JSON reader for the group profile files: objects, arrays, strings
 * (with escapes, \u00XX-\u00FF as Latin-1 bytes), numbers, true/false/null.
 * Strict: anything else is an error naming the position. Pure C99, no I/O. */
#ifndef RP_JSON_H
#define RP_JSON_H

#include <stddef.h>

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } jtype;

typedef struct jnode {
    jtype type;
    char *str;            /* J_STR: the decoded bytes */
    long num;             /* J_NUM (integers only), J_BOOL (0/1) */
    char **keys;          /* J_OBJ, n entries (NULL for J_ARR) */
    struct jnode **items; /* J_OBJ values / J_ARR elements, n entries */
    size_t n;
} jnode;

jnode *json_parse(const char *text, char *err, size_t errcap);
void json_free(jnode *n);

/* NULL when obj is no object or the key is absent. */
const jnode *json_get(const jnode *obj, const char *key);

#endif
