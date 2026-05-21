#include "AudioWorker.h"

#include <portaudio.h>
#include <QDebug>
#include <QMetaType>
#include <QTimer>
#include <algorithm>
#include <vector>

#include "AudioRouting.h"

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

    // Redirect the PulseAudio / PipeWire default source to the chosen monitor
    // *before* PortAudio opens — the open call captures whatever the default
    // is at that moment, and the ALSA host API can't see monitor sources by
    // name. AudioRouting captures the prior default on first use so the
    // crash handler / clean-exit hook can restore it.
    if (!m_monitorSource.isEmpty()) {
        if (!AudioRouting::setDefaultSource(m_monitorSource)) {
            emit errorOccurred(tr("Failed to set system default source to %1")
                                   .arg(m_monitorSource));
            return;
        }
    }

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
            emit bandsReady(m_fft.process(buf));
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
