#include <stdio.h>
#include <stdlib.h>
#include <backtrace.h>

static struct backtrace_state *state = NULL;

static void error_callback(void *data, const char *msg, int errnum) {
    fprintf(stderr, "libbacktrace error: %s (%d)\n", msg, errnum);
}

static int full_callback(void *data, uintptr_t pc,
                         const char *filename, int lineno,
                         const char *function) {
    (void)data;
    if (filename && function) {
        printf("  %s at %s:%d (0x%lx)\n", function, filename, lineno, (unsigned long)pc);
    } else {
        printf("  [0x%lx]\n", (unsigned long)pc);
    }
    return 0;
}

void print_stacktrace(void) {
    if (!state) {
        state = backtrace_create_state(NULL, 1, error_callback, NULL);
    }

    printf("Stack trace:\n");
    backtrace_full(state, 0, full_callback, error_callback, NULL);
}
