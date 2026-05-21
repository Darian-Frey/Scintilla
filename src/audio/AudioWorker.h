#pragma once

#include <QObject>
#include <atomic>
#include <memory>
#include <vector>
#include "FFTProcessor.h"

// Forward declaration so we don't drag portaudio.h into this header.
// PaStreamCallback's full signature requires the typed pointer here —
// the `const void*` abstraction in the original scaffold did not link
// (see BUG-009).
struct PaStreamCallbackTimeInfo;

// ── RingBuffer ────────────────────────────────────────────────────────────────
// Lock-free SPSC ring buffer for float audio samples (DEC-011).
// The PortAudio callback is the producer (real-time thread, no Qt, no allocs).
// AudioWorker::run() is the consumer.

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);

    // Producer — called from PortAudio callback. Returns samples actually written.
    size_t write(const float* data, size_t count);

    // Consumer — called from AudioWorker thread. Returns samples actually read.
    size_t read(float* data, size_t count);

    [[nodiscard]] size_t available() const;

private:
    std::vector<float>     m_buf;
    std::atomic<size_t>    m_head{0};
    std::atomic<size_t>    m_tail{0};
};

// ── AudioWorker ───────────────────────────────────────────────────────────────
//
// Lives on a QThread. Opens a PortAudio capture stream on the selected monitor
// device, drains the ring buffer, runs FFTProcessor, and emits bandsReady().
//
// The PortAudio callback writes into the ring buffer only — no Qt calls (DEC-011).
//
// Typical usage:
//   auto* worker = new AudioWorker(deviceIndex, sampleRate);
//   auto* thread = new QThread;
//   worker->moveToThread(thread);
//   connect(thread, &QThread::started, worker, &AudioWorker::start);
//   connect(worker, &AudioWorker::bandsReady, engine, &AudioReactiveEngine::onBands);
//   thread->start();

class AudioWorker : public QObject {
    Q_OBJECT

public:
    explicit AudioWorker(int deviceIndex, float sampleRate = 44100.0f,
                         const QString& monitorSource = QString(),
                         QObject* parent = nullptr);
    ~AudioWorker() override;

    void setGridSize(int n);
    void setDecayPerFrame(float decay);   // thread-safe — atomic inside FFTProcessor

public slots:
    void start();   // called from QThread::started
    void stop();    // call before thread->quit()

signals:
    void bandsReady(BandData data);
    void errorOccurred(QString message);

private:
    // PortAudio callback — static, real-time thread, no Qt (DEC-011).
    // Signature must match PortAudio's PaStreamCallback typedef exactly;
    // statusFlags is unsigned long (the underlying type of PaStreamCallbackFlags).
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          unsigned long statusFlags,
                          void* userData);

    int         m_deviceIndex;
    float       m_sampleRate;
    QString     m_monitorSource;        // PulseAudio source name, "" = none
    void*       m_paStream = nullptr;   // PaStream* — opaque to avoid pa header here
    RingBuffer  m_ring;
    FFTProcessor m_fft;
    std::atomic<bool> m_running{false};

    static constexpr size_t kRingCapacity = 1024 * 16;  // ~370ms at 44100Hz
};
