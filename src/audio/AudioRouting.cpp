#include "AudioRouting.h"

#include <QProcess>
#include <atomic>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// Buffer that holds the original PulseAudio source name so the signal
// handler can read it without touching malloc / Qt. 512 bytes is roomy —
// real source names rarely exceed 80.
constexpr size_t kBufSize = 512;
char              g_savedSource[kBufSize] = {0};
std::atomic<bool> g_haveSaved{false};

// Capture the current default source into g_savedSource. Idempotent: only
// the first successful call mutates the buffer. Runs synchronously via
// QProcess (called from non-signal context only).
void captureCurrentDefault() {
    if (g_haveSaved.load(std::memory_order_acquire)) return;

    QProcess p;
    p.start(QStringLiteral("pactl"), QStringList{QStringLiteral("get-default-source")});
    if (!p.waitForFinished(2000)) {
        p.kill();
        p.waitForFinished(500);
        return;
    }
    if (p.exitCode() != 0) return;

    const QByteArray result = p.readAllStandardOutput().trimmed();
    if (result.isEmpty()) return;

    const size_t copyLen = std::min(static_cast<size_t>(result.size()), kBufSize - 1);
    std::memcpy(g_savedSource, result.constData(), copyLen);
    g_savedSource[copyLen] = '\0';
    g_haveSaved.store(true, std::memory_order_release);
}

}  // namespace

bool AudioRouting::setDefaultSource(const QString& sourceName) {
    if (sourceName.isEmpty()) return false;

    // Snapshot the original before mutating it, so we can restore later.
    captureCurrentDefault();

    QProcess p;
    p.start(QStringLiteral("pactl"),
            QStringList{QStringLiteral("set-default-source"), sourceName});
    if (!p.waitForFinished(2000)) {
        p.kill();
        p.waitForFinished(500);
        return false;
    }
    return p.exitCode() == 0;
}

void AudioRouting::restoreSaved() {
    if (!g_haveSaved.load(std::memory_order_acquire)) return;

    QProcess p;
    p.start(QStringLiteral("pactl"),
            QStringList{QStringLiteral("set-default-source"),
                        QString::fromUtf8(g_savedSource)});
    p.waitForFinished(2000);
}

void AudioRouting::signalSafeRestore() noexcept {
    if (!g_haveSaved.load(std::memory_order_acquire)) return;

    // Fork + execve. Both async-signal-safe. The child runs `pactl
    // set-default-source <saved>`; the parent waits briefly so we don't
    // _exit before the restore lands.
    const pid_t pid = fork();
    if (pid == 0) {
        // Child. argv needs writable storage; the saved source is already
        // in a writable global buffer.
        char arg0[] = "pactl";
        char arg1[] = "set-default-source";
        char* argv[] = { arg0, arg1, g_savedSource, nullptr };
        execvp("pactl", argv);
        _exit(127);   // exec failed
    } else if (pid > 0) {
        // Parent. Wait for the child to finish so we don't race the abort.
        int status = 0;
        waitpid(pid, &status, 0);
    }
    // On fork failure there's nothing we can safely do — the system stays
    // routed to the monitor. Better than a malloc-in-signal-handler crash.
}

bool AudioRouting::hasSavedSource() {
    return g_haveSaved.load(std::memory_order_acquire);
}

QString AudioRouting::savedSource() {
    if (!g_haveSaved.load(std::memory_order_acquire)) return {};
    return QString::fromUtf8(g_savedSource);
}
