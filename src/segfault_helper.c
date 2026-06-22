/* segfault_helper.c — Hardware fault handling for Aether host targets
 *
 * Uses sigsetjmp/siglongjmp for reliable segfault-to-catch redirection.
 * Stacktrace uses backtrace() + dladdr() on the faulting RIP from ucontext.
 */
#include <signal.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <execinfo.h>

/* Active jump buffer — set by try blocks, cleared after */
static sigjmp_buf *volatile gActiveJmpBuf = NULL;

/* Signal handler */
static void segfaultHandler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;

    /* Extract faulting RIP from ucontext.
     * Verified offsets on macOS 15.7 x86_64:
     *   ucontext_t.uc_mcontext at offset 48 (0x30) — pointer to mcontext_t
     *   mcontext_t data layout: x86_exception_state64_t (16 bytes)
     *     then x86_thread_state64_t (168 bytes)
     *   x86_thread_state64_t.__rip at offset 128 (0x80)
     *   So __rip is at mcontext_data + 16 + 128 = mcontext_data + 144
     *   As uint64_t index: 144/8 = 18 */
    uint64_t faultRip = 0;
#if defined(__APPLE__) && defined(__MACH__)
    volatile uint64_t *mcontext_ptr = (volatile uint64_t *)((volatile char *)ucontext + 48);
    volatile uint64_t *ss = (volatile uint64_t *)(void *)(*mcontext_ptr);
    faultRip = ss[18];  /* offset 144 = 16 (exception) + 128 (rip in thread state) */
#else
    ucontext_t *uc = (ucontext_t *)ucontext;
    faultRip = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
#endif

    /* Print fault info */
    write(2, "\n=== AETHER HARDWARE FAULT ===\n", 32);
    if (info) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "Signal: %d, Fault address: %p\n",
            sig, (void*)info->si_addr);
        if (n > 0 && n < 128) write(2, buf, (size_t)n);
    }

    /* Resolve the faulting RIP to a function name */
    {
        char buf[512];
        Dl_info dlinfo;
        int n;
        if (dladdr((void*)(uintptr_t)faultRip, &dlinfo) && dlinfo.dli_sname) {
            ptrdiff_t offset = (uintptr_t)faultRip - (uintptr_t)dlinfo.dli_saddr;
            n = snprintf(buf, sizeof(buf), "  Crash in: %s + %td  (RIP: 0x%016" PRIx64 ")\n",
                dlinfo.dli_sname, offset, faultRip);
        } else {
            n = snprintf(buf, sizeof(buf), "  Crash at RIP: 0x%016" PRIx64 " (unknown function)\n",
                faultRip);
        }
        write(2, buf, (size_t)(n > 0 && n < (int)sizeof(buf) ? n : 0));
    }

    /* Print backtrace from current context */
    write(2, "Backtrace:\n", 11);
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    backtrace_symbols_fd(callstack, frames, 2);

    write(2, "============================\n\n", 30);

    /* If inside a try block, longjmp to catch handler */
    if (gActiveJmpBuf) {
        siglongjmp(gActiveJmpBuf[0], 1);
    }

    _exit(139);
}

int aether_setJmpBuf(sigjmp_buf *buf) {
    gActiveJmpBuf = buf;
    int result = sigsetjmp(buf[0], 0);
    if (result != 0) {
        gActiveJmpBuf = NULL;
    }
    return result;
}

void aether_clearJmpBuf(void) {
    gActiveJmpBuf = NULL;
}

void aether_initSegfault(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segfaultHandler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

void aether_printStacktrace(void) {
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    backtrace_symbols_fd(callstack, frames, 2);
}
