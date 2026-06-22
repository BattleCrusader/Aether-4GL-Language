/* segfault_helper.c — Hardware fault handling for Aether host targets
 *
 * Uses sigsetjmp/siglongjmp for reliable segfault-to-catch redirection.
 * The generated assembly calls aether_setJmpBuf() before the try body.
 * sigsetjmp saves the stack context. On segfault, the signal handler
 * calls siglongjmp to jump back to the saved context, and the C function
 * returns 1 to the generated assembly, which then jumps to the catch handler.
 *
 * This avoids fragile ucontext struct offset manipulation.
 */
#include <signal.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

/* Active jump buffer — set by try blocks, cleared after */
static sigjmp_buf *volatile gActiveJmpBuf = NULL;

/* Signal handler — prints stacktrace, then siglongjmps to catch handler */
static void segfaultHandler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;
    (void)ucontext;

    /* Print fault info */
    write(2, "\n=== AETHER HARDWARE FAULT ===\n", 32);
    if (info) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "Fault address: %p\n", (void*)info->si_addr);
        if (n > 0 && n < 64) write(2, buf, n);
    }
    write(2, "Stack trace:\n", 13);
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    backtrace_symbols_fd(callstack, frames, 2);
    write(2, "============================\n\n", 30);

    /* If inside a try block, longjmp to catch handler.
     * siglongjmp restores the stack to where sigsetjmp was called,
     * and makes sigsetjmp return the value 1 (instead of 0).
     * The C function aether_setJmpBuf then returns 1 to the
     * generated assembly, which jumps to the catch handler. */
    if (gActiveJmpBuf) {
        siglongjmp(gActiveJmpBuf[0], 1);
    }

    /* No catch handler — exit */
    _exit(139);
}

/* Called from generated assembly before try body.
 * sigsetjmp(buf, 0) saves stack context WITHOUT saving signal mask.
 * Returns 0 on first call, non-zero if siglongjmp was called (segfault caught). */
int aether_setJmpBuf(sigjmp_buf *buf) {
    gActiveJmpBuf = buf;
    int result = sigsetjmp(buf[0], 0);
    if (result != 0) {
        /* We got here via siglongjmp from the signal handler.
         * Clear the active buffer and return 1 to indicate segfault. */
        gActiveJmpBuf = NULL;
    }
    return result;
}

/* Called from generated assembly after try body (normal path) */
void aether_clearJmpBuf(void) {
    gActiveJmpBuf = NULL;
}

/* Called once at program startup to register the SIGSEGV handler */
void aether_initSegfault(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segfaultHandler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

/* Print a stacktrace from Aether code */
void aether_printStacktrace(void) {
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    backtrace_symbols_fd(callstack, frames, 2);
}
