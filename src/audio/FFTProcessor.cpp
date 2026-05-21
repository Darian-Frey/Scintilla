#include "FFTProcessor.h"

#include "kiss_fft.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    // Audible-range edges (Hz); per DEC-015 we log-space N+1 edges in this range.
    constexpr float kFreqMin = 20.0f;
    constexpr float kFreqMax = 20000.0f;
}

// ── KissState opaque wrapper ─────────────────────────────────────────────────

struct FFTProcessor::KissState {
    kiss_fft_cfg cfg = nullptr;
    kiss_fft_cpx in[kFftSize]  = {};
    kiss_fft_cpx out[kFftSize] = {};
};

// ── Construction ─────────────────────────────────────────────────────────────

FFTProcessor::FFTProcessor(float sampleRate)
    : m_sampleRate(sampleRate)
    , m_kiss(new KissState) {
    m_kiss->cfg = kiss_fft_alloc(kFftSize, /*inverse*/ 0, nullptr, nullptr);
    buildWindow();
    buildBandEdges();
}

FFTProcessor::~FFTProcessor() {
    if (m_kiss && m_kiss->cfg) {
        kiss_fft_free(m_kiss->cfg);
    }
    delete m_kiss;
}

// ── Configuration ────────────────────────────────────────────────────────────

void FFTProcessor::setSampleRate(float sr) {
    if (std::abs(sr - m_sampleRate) < 1e-3f) return;
    m_sampleRate = sr;
    buildBandEdges();
    resetNormalisation();
}

void FFTProcessor::setGridSize(int n) {
    n = std::clamp(n, 3, kNumBands);
    if (n == m_gridSize) return;
    m_gridSize = n;
    buildBandEdges();
    resetNormalisation();
}

void FFTProcessor::resetNormalisation() {
    m_rollingMax.fill(0.0f);
    m_prevEnergy = 0.0f;
}

void FFTProcessor::setDecayPerFrame(float decay) {
    m_decayPerFrame.store(std::clamp(decay, 0.5f, 0.9999f), std::memory_order_relaxed);
}

// ── Per-frame processing ─────────────────────────────────────────────────────

BandData FFTProcessor::process(std::span<const float> samples) {
    BandData out;

    if (samples.size() < static_cast<size_t>(kFftSize)) {
        return out;  // not enough data; caller should buffer
    }

    // Window + load complex input (real part = sample * window, imag = 0).
    float rmsSum = 0.0f;
    for (int i = 0; i < kFftSize; ++i) {
        const float s = samples[static_cast<size_t>(i)] * m_window[static_cast<size_t>(i)];
        m_kiss->in[i].r = s;
        m_kiss->in[i].i = 0.0f;
        rmsSum += samples[static_cast<size_t>(i)] * samples[static_cast<size_t>(i)];
    }
    out.rms = std::sqrt(rmsSum / static_cast<float>(kFftSize));

    kiss_fft(m_kiss->cfg, m_kiss->in, m_kiss->out);

    // Magnitudes for the positive-frequency half (bins 0..kFftSize/2).
    // We accumulate per log-spaced band and divide by bin count to get an
    // average magnitude per band.
    constexpr int kNyquistBin = kFftSize / 2;

    // Reset active bands; inactive bands stay at 0.
    for (int b = 0; b < m_gridSize; ++b) out.bands[static_cast<size_t>(b)] = 0.0f;

    // Spectral centroid: sum(f * mag) / sum(mag) over all positive bins,
    // then normalise to [0, 1] against kFreqMax.
    float centroidNum = 0.0f;
    float centroidDen = 0.0f;

    int   bandIdx     = 0;
    float bandAcc     = 0.0f;
    int   bandCount   = 0;
    float bandEdgeHi  = m_bandEdges[1];

    for (int bin = 1; bin <= kNyquistBin; ++bin) {
        const float re  = m_kiss->out[bin].r;
        const float im  = m_kiss->out[bin].i;
        const float mag = std::sqrt(re * re + im * im);
        const float hz  = binToHz(bin);

        centroidNum += hz * mag;
        centroidDen += mag;

        // Advance band index while this bin is above the current band's upper edge.
        while (bandIdx < m_gridSize - 1 && hz >= bandEdgeHi) {
            if (bandCount > 0) {
                out.bands[static_cast<size_t>(bandIdx)] = bandAcc / static_cast<float>(bandCount);
            }
            bandAcc   = 0.0f;
            bandCount = 0;
            ++bandIdx;
            bandEdgeHi = m_bandEdges[static_cast<size_t>(bandIdx + 1)];
        }
        bandAcc += mag;
        ++bandCount;
    }
    if (bandCount > 0 && bandIdx < m_gridSize) {
        out.bands[static_cast<size_t>(bandIdx)] = bandAcc / static_cast<float>(bandCount);
    }

    // Normalise against rolling max per band (DEC-015). Decay is atomic so
    // the main thread can adjust "smoothing" live without racing this loop.
    const float decay = m_decayPerFrame.load(std::memory_order_relaxed);
    for (int b = 0; b < m_gridSize; ++b) {
        const size_t i = static_cast<size_t>(b);
        m_rollingMax[i] = std::max(m_rollingMax[i] * decay, out.bands[i]);
        const float ref = std::max(m_rollingMax[i], 1e-6f);
        out.bands[i] = std::clamp(out.bands[i] / ref, 0.0f, 1.0f);
    }

    out.centroid = (centroidDen > 1e-6f)
                       ? std::clamp((centroidNum / centroidDen) / kFreqMax, 0.0f, 1.0f)
                       : 0.0f;

    // Onset detection: ratio of low-band energy (first quarter) to its running
    // average. Spikes mean a beat.
    float lowEnergy = 0.0f;
    const int lowEnd = std::max(1, m_gridSize / 4);
    for (int b = 0; b < lowEnd; ++b) {
        lowEnergy += out.bands[static_cast<size_t>(b)];
    }
    if (m_prevEnergy > 1e-6f) {
        out.beat = (lowEnergy / m_prevEnergy) > m_beatThreshold;
    }
    m_prevEnergy = 0.9f * m_prevEnergy + 0.1f * lowEnergy;

    return out;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void FFTProcessor::buildWindow() {
    // Hann window: 0.5 * (1 - cos(2π n / (N-1)))
    for (int n = 0; n < kFftSize; ++n) {
        m_window[static_cast<size_t>(n)] =
            0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(n)
                                          / static_cast<float>(kFftSize - 1)));
    }
}

void FFTProcessor::buildBandEdges() {
    // Log-spaced edges: f_k = f_min * (f_max / f_min)^(k/N)  for k = 0..N
    const float lo  = kFreqMin;
    const float hi  = std::min(kFreqMax, m_sampleRate * 0.5f);
    const float lr  = std::log(hi / lo);

    for (int k = 0; k <= m_gridSize; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(m_gridSize);
        m_bandEdges[static_cast<size_t>(k)] = lo * std::exp(t * lr);
    }
    // Fill remaining slots with hi so unused bands still have sensible edges.
    for (int k = m_gridSize + 1; k <= kNumBands; ++k) {
        m_bandEdges[static_cast<size_t>(k)] = hi;
    }
}

float FFTProcessor::binToHz(int bin) const {
    return (static_cast<float>(bin) * m_sampleRate) / static_cast<float>(kFftSize);
}

int FFTProcessor::hzToBin(float hz) const {
    return static_cast<int>(std::round(hz * static_cast<float>(kFftSize) / m_sampleRate));
}
