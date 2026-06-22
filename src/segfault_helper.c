/* segfault_helper.c — Hardware fault handling for Aether host targets */
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

void *volatile __aether_faultReturnAddr = NULL;
void *volatile __aether_faultSavedRsp = NULL;
void *volatile __aether_faultSavedRbp = NULL;

static void segfaultHandler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;

    void *retAddr = __aether_faultReturnAddr;
    void *savedRsp = __aether_faultSavedRsp;
    void *savedRbp = __aether_faultSavedRbp;
    __aether_faultReturnAddr = NULL;

    /* Print fault info using write() */
    write(2, "\n=== AETHER HARDWARE FAULT ===\n", 32);
    if (info) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "Fault address: %p\n", (void*)info->si_addr);
        write(2, buf, (n > 0 && n < 64) ? n : 0);
    }
    write(2, "Stack trace:\n", 13);
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    backtrace_symbols_fd(callstack, frames, 2);
    write(2, "============================\n\n", 30);

    if (retAddr) {
#if defined(__APPLE__) && defined(__MACH__)
        ucontext_t *uc = (ucontext_t *)ucontext;
        uc->uc_mcontext->__ss.__rip = (uint64_t)retAddr;
        uc->uc_mcontext->__ss.__rax = 1;
        uc->uc_mcontext->__ss.__rdx = 1;
        if (savedRsp) uc->uc_mcontext->__ss.__rsp = (uint64_t)savedRsp;
        if (savedRbp) uc->uc_mcontext->__ss.__rbp = (uint64_t)savedRbp;
#else
        ucontext_t *uc = (ucontext_t *)ucontext;
        uc->uc_mcontext.gregs[REG_RIP] = (uint64_t)retAddr;
        uc->uc_mcontext.gregs[REG_RAX] = 1;
        uc->uc_mcontext.gregs[REG_RDX] = 1;
        if (savedRsp) uc->uc_mcontext.gregs[REG_RSP] = (uint64_t)savedRsp;
        if (savedRbp) uc->uc_mcontext.gregs[REG_RBP] = (uint64_t)savedRbp;
#endif
    } else {
        /* No catch handler — _exit is async-signal-safe */
        _exit(139);
    }
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
