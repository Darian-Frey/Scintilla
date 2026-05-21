#pragma once

// ── CrashHandler ─────────────────────────────────────────────────────────────
//
// POSIX signal-based crash handler. Installs handlers for SIGSEGV / SIGABRT /
// SIGFPE / SIGILL / SIGBUS, dumps a stack trace to stderr via
// backtrace_symbols_fd, runs every registered async-signal-safe hook in
// registration order, then re-raises the signal so the OS still produces a
// coredump.
//
// Hooks must be strictly async-signal-safe — they run from inside the signal
// handler. That means: no malloc, no Qt calls, no QProcess. Use only POSIX
// async-signal-safe functions (write, fork, execve, _exit, waitpid, etc.).
//
// For non-signal cleanup, connect to QApplication::aboutToQuit instead;
// that path has the full Qt runtime available.

class CrashHandler {
public:
    using AsyncSafeHook = void(*)() noexcept;

    // Install signal handlers. Call once, very early in main() — before
    // QApplication is constructed so the handler is active during Qt's own
    // setup work.
    static void install();

    // Register a hook to run when a fatal signal fires. Up to a small fixed
    // number of hooks; additional registrations beyond the cap are ignored.
    // Returns true on success.
    static bool addCrashHook(AsyncSafeHook hook);
};
