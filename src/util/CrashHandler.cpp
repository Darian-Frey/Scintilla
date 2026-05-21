#include "CrashHandler.h"

#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>

namespace {

constexpr size_t kMaxHooks = 16;
std::array<CrashHandler::AsyncSafeHook, kMaxHooks> g_hooks{};
std::atomic<size_t>                                g_hookCount{0};

// Async-signal-safe alternative to strlen.
size_t asLen(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

// Write a literal C string to a fd; signal-safe.
void asWrite(int fd, const char* s) {
    const auto n = asLen(s);
    ssize_t written = 0;
    while (written < static_cast<ssize_t>(n)) {
        const auto r = ::write(fd, s + written, n - static_cast<size_t>(written));
        if (r <= 0) return;
        written += r;
    }
}

// Write a small unsigned int as decimal to a fd; signal-safe.
void asWriteUInt(int fd, unsigned v) {
    char buf[16];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    if (v == 0) {
        *--p = '0';
    } else {
        while (v > 0 && p > buf) {
            *--p = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
    }
    asWrite(fd, p);
}

const char* signalName(int signo) {
    switch (signo) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        case SIGBUS:  return "SIGBUS";
        default:      return "UNKNOWN";
    }
}

void crashHandler(int signo) noexcept {
    // 1) Banner
    asWrite(STDERR_FILENO, "\n=== Scintilla crashed (");
    asWrite(STDERR_FILENO, signalName(signo));
    asWrite(STDERR_FILENO, " / signal ");
    asWriteUInt(STDERR_FILENO, static_cast<unsigned>(signo));
    asWrite(STDERR_FILENO, ") ===\nStack trace:\n");

    // 2) Stack trace. backtrace_symbols_fd is documented as not calling
    //    malloc (unlike backtrace_symbols), so it is safe in a signal
    //    handler. Symbols from the main binary appear when the executable
    //    was linked with -rdynamic; see CMakeLists.txt.
    void* frames[64];
    const int frameCount = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);

    // 3) Run hooks (audio routing restore lives here).
    const size_t count = g_hookCount.load(std::memory_order_acquire);
    for (size_t i = 0; i < count && i < kMaxHooks; ++i) {
        if (g_hooks[i]) g_hooks[i]();
    }

    // 4) Re-raise with the default handler so the OS can still produce a
    //    coredump for post-mortem debugging.
    asWrite(STDERR_FILENO, "Re-raising for coredump.\n");
    std::signal(signo, SIG_DFL);
    std::raise(signo);
}

}  // namespace

void CrashHandler::install() {
    struct sigaction sa{};
    sa.sa_handler = &crashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;   // allow nested same-signal so re-raise works

    for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS }) {
        sigaction(sig, &sa, nullptr);
    }
}

bool CrashHandler::addCrashHook(AsyncSafeHook hook) {
    if (!hook) return false;
    const size_t idx = g_hookCount.fetch_add(1, std::memory_order_acq_rel);
    if (idx >= kMaxHooks) return false;
    g_hooks[idx] = hook;
    return true;
}
