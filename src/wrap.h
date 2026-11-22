#ifndef RP_WRAP_H
#define RP_WRAP_H

#include <stddef.h>

typedef struct {
    char **lines;
    size_t n;
} rp_lines;

/* Greedy word wrap on whitespace runs. The first line may have a different
 * width from the rest; a word longer than the width stands alone rather than
 * splitting. Empty text renders one empty line. Caller frees with rp_free_lines. */
rp_lines rp_wrap_words(const char *text, int first_width, int cont_width);
void rp_free_lines(rp_lines *l);

#endif
