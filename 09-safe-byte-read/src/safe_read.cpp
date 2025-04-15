#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "safe_read.h"

using namespace std;

namespace {

static sigjmp_buf JMP;

static void safe_handler(int signo, siginfo_t *info, void *extra) {
    siglongjmp(JMP, 1);
}

} // namespace

optional<uint8_t> safe_read_byte(const uint8_t *ptr) {
    struct sigaction action, old_segv_action, olg_segb_action;
    action.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    action.sa_sigaction = safe_handler;
    sigaction(SIGSEGV, &action, &old_segv_action);
    sigaction(SIGBUS, &action, &olg_segb_action);

    if (sigsetjmp(JMP, 1)) {
        sigaction(SIGSEGV, &old_segv_action, NULL);
        sigaction(SIGBUS, &olg_segb_action, NULL);
        return nullopt;
    } else {
        uint8_t byte = *ptr;
        sigaction(SIGSEGV, &old_segv_action, NULL);
        sigaction(SIGBUS, &olg_segb_action, NULL);
        return byte;
    }
}
