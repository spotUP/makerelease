#include "ui.h"

/* libnix startup swaps to this stack when the caller gave less (MUI custom classes need 48 KB) */
unsigned long __stack = 49152;
/* $STACK cookie: the OS 3.2 Shell reads it and starts the program with this stack */
#ifdef APP_VER_STRING
static const char __attribute__((used)) version[] = "\0" APP_VER_STRING;
#endif
static const char __attribute__((used)) stack_cookie[] = "$STACK:49152";

int main(int argc, char **argv) {
    return rp_ui_run(argc, argv);
}
