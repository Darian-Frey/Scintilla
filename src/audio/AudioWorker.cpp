#include "AudioWorker.h"

#include <portaudio.h>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaType>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <vector>

// ── RingBuffer ───────────────────────────────────────────────────────────────
//
// SPSC ring buffer (DEC-011 / AV-008). The producer (PortAudio callback)
// reads m_tail with acquire semantics and writes m_head with release. The
// consumer does the reverse. Capacity is power-of-two so we can use bitmask
// modulo. No allocations after construction; no mutexes anywhere.

namespace {
    constexpr size_t roundUpPow2(size_t n) {
        size_t r = 1;
        while (r < n) r <<= 1;
        return r;
    }
}

RingBuffer::RingBuffer(size_t capacity)
    : m_buf(roundUpPow2(capacity), 0.0f) {}

size_t RingBuffer::write(const float* data, size_t count) {
    const size_t cap  = m_buf.size();
    const size_t mask = cap - 1;
    const size_t tail = m_tail.load(std::memory_order_acquire);
    size_t       head = m_head.load(std::memory_order_relaxed);

    const size_t free = cap - (head - tail);
    const size_t n    = std::min(count, free);

    for (size_t i = 0; i < n; ++i) {
        m_buf[(head + i) & mask] = data[i];
    }
    m_head.store(head + n, std::memory_order_release);
    return n;
}

size_t RingBuffer::read(float* data, size_t count) {
    const size_t cap  = m_buf.size();
    const size_t mask = cap - 1;
    const size_t head = m_head.load(std::memory_order_acquire);
    size_t       tail = m_tail.load(std::memory_order_relaxed);

    const size_t used = head - tail;
    const size_t n    = std::min(count, used);

    for (size_t i = 0; i < n; ++i) {
        data[i] = m_buf[(tail + i) & mask];
    }
    m_tail.store(tail + n, std::memory_order_release);
    return n;
}

size_t RingBuffer::available() const {
    const size_t head = m_head.load(std::memory_order_acquire);
    const size_t tail = m_tail.load(std::memory_order_acquire);
    return head - tail;
}

// ── AudioWorker ──────────────────────────────────────────────────────────────

AudioWorker::AudioWorker(int deviceIndex, float sampleRate,
                         const QString& monitorSource, QObject* parent)
    : QObject(parent)
    , m_deviceIndex(deviceIndex)
    , m_sampleRate(sampleRate)
    , m_monitorSource(monitorSource)
    , m_ring(kRingCapacity)
    , m_fft(sampleRate) {
}

AudioWorker::~AudioWorker() {
    stop();
}

void AudioWorker::setGridSize(int n) {
    m_fft.setGridSize(n);
}

void AudioWorker::setDecayPerFrame(float decay) {
    m_fft.setDecayPerFrame(decay);
}

// ── PortAudio callback ───────────────────────────────────────────────────────
//
// Runs on PortAudio's audio thread. The DEC-011 / AV-008 invariant: no Qt
// calls, no allocations, no mutex acquisition. We do exactly one thing —
// write the incoming mono samples into the SPSC ring.

int AudioWorker::paCallback(const void* input, void* /*output*/,
                            unsigned long frameCount,
                            const PaStreamCallbackTimeInfo* /*timeInfo*/,
                            unsigned long /*statusFlags*/,
                            void* userData) {
    auto* self = static_cast<AudioWorker*>(userData);
    const float* in = static_cast<const float*>(input);
    if (in && self->m_running.load(std::memory_order_acquire)) {
        // Caller opens the stream as mono; if the source was multi-channel
        // PortAudio will already have downmixed to one channel for us when we
        // request `channelCount = 1` (it picks the first channel by default).
        self->m_ring.write(in, frameCount);
    }
    return paContinue;
}

// ── Lifecycle (slots) ────────────────────────────────────────────────────────

void AudioWorker::start() {
    if (m_paStream) return;

    qDebug().noquote() << "[audio] AudioWorker::start"
                       << "  deviceIndex(initial)=" << m_deviceIndex
                       << "  sampleRate="          << m_sampleRate
                       << "  monitorSource="       << (m_monitorSource.isEmpty()
                                                       ? QStringLiteral("(none)")
                                                       : m_monitorSource);

    if (Pa_Initialize() != paNoError) {
        emit errorOccurred(tr("Pa_Initialize failed"));
        return;
    }

    // If a monitor was just set as default, ignore the caller's deviceIndex
    // and open PortAudio's notion of the default input — that's the one that
    // now points at the monitor.
    if (!m_monitorSource.isEmpty()) {
        const PaDeviceIndex def = Pa_GetDefaultInputDevice();
        if (def != paNoDevice) m_deviceIndex = def;
        qDebug().noquote() << "[audio] resolved monitor-route to PortAudio default device"
                           << "index=" << m_deviceIndex;
    }

    PaStreamParameters params{};
    params.device                    = m_deviceIndex;
    params.channelCount              = 1;
    params.sampleFormat              = paFloat32;
    params.suggestedLatency          = Pa_GetDeviceInfo(m_deviceIndex)
                                          ? Pa_GetDeviceInfo(m_deviceIndex)->defaultLowInputLatency
                                          : 0.020;
    params.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream = nullptr;
    const auto err = Pa_OpenStream(&stream,
                                   &params,
                                   nullptr,                           // output: none
                                   static_cast<double>(m_sampleRate),
                                   /*framesPerBuffer*/ 512,
                                   paClipOff,
                                   &AudioWorker::paCallback,
                                   this);
    if (err != paNoError) {
        emit errorOccurred(tr("Pa_OpenStream failed: %1").arg(Pa_GetErrorText(err)));
        Pa_Terminate();
        return;
    }

    m_paStream = stream;
    m_running.store(true, std::memory_order_release);
    m_fft.resetNormalisation();

    if (Pa_StartStream(stream) != paNoError) {
        emit errorOccurred(tr("Pa_StartStream failed"));
        Pa_CloseStream(stream);
        m_paStream = nullptr;
        m_running.store(false, std::memory_order_release);
        Pa_Terminate();
        return;
    }

    // Per-stream routing for system-audio capture (BUG-013): on PipeWire
    // systems the ALSA pcm.pipewire plugin bypasses libpulse, so PULSE_SOURCE
    // and pactl set-default-source don't reliably redirect *this* stream.
    // Diagnostic tests showed `pactl move-source-output` works per-stream
    // with no global side effects, so we use that after Pa_StartStream:
    //
    //   1. Open PortAudio normally — binds source-output to whatever PipeWire
    //      picks (usually the system default mic).
    //   2. Poll `pactl list source-outputs` for ours, identified by the
    //      "PipeWire ALSA [Scintilla]" substring in the application.name
    //      property — the pcm.pipewire plugin doesn't set
    //      application.process.id, so we can't match by PID (BUG-014).
    //   3. `pactl move-source-output <id> <monitor>` to redirect just our
    //      stream. The redirect lives only as long as the stream; nothing
    //      to undo on stop, nothing to restore on crash.
    if (!m_monitorSource.isEmpty()) {
        // application.name is "PipeWire ALSA [<binary>]" — match the binary
        // suffix verbatim. Verified against the diagnostic capture in
        // /home/azathoth/Scintilla session.
        const QString appNameMarker = QStringLiteral("[Scintilla]");
        QString outputId;

        // PipeWire registers the source-output shortly after Pa_StartStream;
        // poll up to ~3 s in 50 ms intervals (PortAudio's first connection
        // can take noticeably longer than aplay's, so 1 s wasn't enough).
        for (int tries = 0; tries < 60 && outputId.isEmpty(); ++tries) {
            QProcess listProc;
            listProc.start(QStringLiteral("pactl"),
                           QStringList{QStringLiteral("list"),
                                       QStringLiteral("source-outputs")});
            if (!listProc.waitForFinished(2000)) {
                listProc.kill();
                listProc.waitForFinished(200);
            } else if (listProc.exitCode() == 0) {
                const QString text  = QString::fromUtf8(listProc.readAllStandardOutput());
                const QStringList chunks =
                    text.split(QStringLiteral("Source Output #"), Qt::SkipEmptyParts);
                for (const QString& chunk : chunks) {
                    if (!chunk.contains(appNameMarker)) continue;
                    const int nl = chunk.indexOf('\n');
                    outputId = chunk.left(nl).trimmed();
                    break;
                }
            }
            if (outputId.isEmpty()) QThread::msleep(50);
        }

        if (outputId.isEmpty()) {
            qWarning() << "[audio] Could not find own source-output after 3s";
            emit errorOccurred(
                tr("Couldn't find Scintilla's source-output after 3s; "
                   "capture will use the system default input instead."));
        } else {
            qDebug().noquote() << "[audio] found own source-output id=" << outputId
                               << "  moving to" << m_monitorSource;
            QProcess moveProc;
            moveProc.start(QStringLiteral("pactl"),
                           QStringList{QStringLiteral("move-source-output"),
                                       outputId, m_monitorSource});
            if (!moveProc.waitForFinished(2000) || moveProc.exitCode() != 0) {
                qWarning().noquote()
                    << "[audio] move-source-output failed:"
                    << QString::fromUtf8(moveProc.readAllStandardError()).trimmed();
                emit errorOccurred(
                    tr("pactl move-source-output failed: %1")
                        .arg(QString::fromUtf8(moveProc.readAllStandardError()).trimmed()));
            } else {
                qDebug() << "[audio] move-source-output succeeded";
            }
        }

        // Discard the few hundred ms of pre-move data captured from the
        // system default so the visualisation starts cleanly on the monitor.
        const size_t available = m_ring.available();
        if (available > 0) {
            std::vector<float> dump(available);
            m_ring.read(dump.data(), available);
        }
    }

    // Drain the ring on a Qt timer running on this worker's thread. The
    // worker is moved-to-thread before start() fires, so this timer ticks
    // on the worker thread (not the GUI thread or the PA callback thread).
    auto* timer = new QTimer(this);
    timer->setInterval(8);   // ~120 Hz polling, FFT processes every kFftSize samples
    connect(timer, &QTimer::timeout, this, [this]() {
        std::vector<float> buf;
        buf.resize(FFTProcessor::kFftSize);
        while (m_ring.available() >= FFTProcessor::kFftSize) {
            m_ring.read(buf.data(), FFTProcessor::kFftSize);
            const auto bands = m_fft.process(buf);
            ++m_diagFrame;
            // One-line health check every ~5 s of audio so the user can see
            // whether real data is flowing into the engine. Cadence picked
            // to be visible-but-quiet — drop to 50 if you're actively
            // debugging silent capture.
            if (m_diagFrame % 250 == 0) {
                float maxBand = 0.0f;
                for (float b : bands.bands) if (b > maxBand) maxBand = b;
                qDebug().noquote()
                    << "[audio] frame" << m_diagFrame
                    << " rms="         << QString::number(bands.rms, 'f', 4)
                    << " maxBand="     << QString::number(maxBand, 'f', 3)
                    << " centroid="    << QString::number(bands.centroid, 'f', 3)
                    << " beat="        << (bands.beat ? "Y" : ".");
            }
            emit bandsReady(bands);
        }
    });
    timer->start();
}

void AudioWorker::stop() {
    if (!m_paStream) return;
    m_running.store(false, std::memory_order_release);

    auto* stream = static_cast<PaStream*>(m_paStream);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    m_paStream = nullptr;
    Pa_Terminate();
}
