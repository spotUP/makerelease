/* Aminet upload directories (port of app/src/shared/aminet-types.ts and aminet-guess.ts). */
#ifndef RP_AMINET_TYPES_H
#define RP_AMINET_TYPES_H

#define RP_AMINET_TYPES_MAX 400   /* capacity for a cycle label array; the list has 362 */

/* Directory strings ("demo/aga") and their descriptions, parallel, both NULL-terminated; usable as cycle entries. */
extern const char *const rp_aminet_types[];
extern const char *const rp_aminet_type_descs[];

int rp_aminet_type_count(void);
/* Index of an exact directory string in rp_aminet_types, or -1. */
int rp_aminet_type_index(const char *type);
/* guessAminetType: best directory for a release category and platform tag ("A500", "A1200", ...), or "" when none fits. */
const char *rp_aminet_guess_type(const char *category, const char *platform_tag);

#endif
