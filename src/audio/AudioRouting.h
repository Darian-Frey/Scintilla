#pragma once

#include <QString>

// ── AudioRouting ─────────────────────────────────────────────────────────────
//
// Owns the saved-original-default-source state and the pactl invocations
// that mutate it. Two restore paths:
//
//   restoreSaved()        — for normal exit (QApplication::aboutToQuit).
//                           Uses QProcess; full Qt runtime available.
//   signalSafeRestore()   — for the crash path. Async-signal-safe only:
//                           fork() + execve() + waitpid(). No Qt, no malloc.
//
// First call to setDefaultSource() captures the current system default
// before mutating it; subsequent calls don't overwrite the saved value, so
// restore always returns the system to the pre-Scintilla state.

class AudioRouting {
public:
    // Set the system default PulseAudio / PipeWire source. On first call,
    // captures the current default into a global buffer so restoreSaved /
    // signalSafeRestore can put it back. Returns true on success.
    static bool setDefaultSource(const QString& sourceName);

    // Restore the saved default source via QProcess. Safe for normal exit.
    // No-op if nothing was ever saved.
    static void restoreSaved();

    // Restore the saved default source via fork+execve. Safe to call from
    // a POSIX signal handler. No-op if nothing was ever saved.
    static void signalSafeRestore() noexcept;

    // True iff setDefaultSource has been called at least once this session.
    [[nodiscard]] static bool hasSavedSource();

    // The saved source name as a QString — convenience for status bar etc.
    [[nodiscard]] static QString savedSource();
};
