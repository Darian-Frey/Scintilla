#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

// ── BandData ──────────────────────────────────────────────────────────────────
// Produced once per FFT frame. Fixed 32-entry array; only [0..gridSize) are used.
// All values normalised to [0, 1] via rolling-max with 3-second decay (DEC-015).

struct BandData {
    std::array<float, 32> bands  = {};  // log-spaced frequency band magnitudes
    float                 rms    = 0;   // overall RMS amplitude
    float                 centroid = 0; // spectral centroid, normalised [0, 1]
    bool                  beat   = false; // onset detected this frame
};

// ── FFTProcessor ──────────────────────────────────────────────────────────────
//
// Owns the KissFFT state. Takes a chunk of mono float samples, applies a
// Hann window, runs the FFT, maps bins to log-spaced bands, and returns BandData.
//
// Not thread-safe — call only from the AudioWorker thread.

class FFTProcessor {
public:
    static constexpr int kFftSize   = 1024;
    static constexpr int kNumBands  = 32;   // max; actual used = gridSize

    explicit FFTProcessor(float sampleRate = 44100.0f);
    ~FFTProcessor();

    // Process a block of mono samples. Returns BandData.
    // Input must have at least kFftSize samples; only the first kFftSize are used.
    [[nodiscard]] BandData process(std::span<const float> samples);

    void setSampleRate(float sr);
    void setGridSize(int n);   // affects how many bands are active

    // Rolling-max decay per frame for band normalisation (DEC-015).
    // Range [0.5, 0.9999]. Lower = snappier band reactivity; higher = slower
    // adaptation to changes in volume. Safe to call from any thread —
    // the value is read atomically in process().
    void setDecayPerFrame(float decay);
    [[nodiscard]] float decayPerFrame() const { return m_decayPerFrame.load(std::memory_order_relaxed); }

    // Reset rolling normalisation (call on stream restart)
    void resetNormalisation();

private:
    float m_sampleRate  = 44100.0f;
    int   m_gridSize    = 8;

    // KissFFT state (opaque pointer to avoid including kiss_fft.h in this header)
    struct KissState;
    KissState* m_kiss = nullptr;

    // Pre-computed Hann window
    std::array<float, kFftSize> m_window = {};

    // Log-spaced band edges (Hz), computed from gridSize
    std::array<float, kNumBands + 1> m_bandEdges = {};

    // Rolling max per band (for normalisation, DEC-015)
    std::array<float, kNumBands> m_rollingMax = {};
    // Atomic so the main thread can tune decay live without racing the audio
    // thread's process() reads. Initialised to ~3s half-life at 50fps drain.
    std::atomic<float> m_decayPerFrame{0.9985f};

    // Onset detection state
    float m_prevEnergy    = 0.0f;
    float m_beatThreshold = 1.4f; // energy ratio to declare a beat

    void buildWindow();
    void buildBandEdges();
    [[nodiscard]] float binToHz(int bin) const;
    [[nodiscard]] int   hzToBin(float hz) const;
};
