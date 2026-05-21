#include "AudioReactiveEngine.h"

#include <portaudio.h>
#include <QMetaType>
#include <QProcess>
#include <algorithm>
#include <cmath>

namespace {
    inline uint8_t clamp255(int v) {
        return static_cast<uint8_t>(std::clamp(v, 0, 255));
    }
}

// ── Construction ─────────────────────────────────────────────────────────────

AudioReactiveEngine::AudioReactiveEngine(QObject* parent)
    : QObject(parent) {
    // Register types so queued connections from the worker thread can
    // marshal BandData and VoxelFrame across the thread boundary.
    qRegisterMetaType<BandData>("BandData");
    qRegisterMetaType<VoxelFrame>("VoxelFrame");
}

AudioReactiveEngine::~AudioReactiveEngine() {
    stop();
}

// ── Configuration ────────────────────────────────────────────────────────────

void AudioReactiveEngine::setMask(std::shared_ptr<const ShapeMask> mask) {
    m_mask = std::move(mask);
    if (m_worker && m_mask) m_worker->setGridSize(m_mask->gridSize());
    // Grid size may have changed; clear all mode-local rolling state so
    // (z,x)-indexed buffers don't linger at the old size.
    m_waveformHistory.clear();
    m_waveformFrameCounter = 0;
    m_tunnelHistory.clear();
    m_tunnelHead = 0;
}

void AudioReactiveEngine::setMode(ReactiveMode mode) {
    m_mode = mode;
    if (mode != ReactiveMode::BeatPulse)     m_pulse = {};
    if (mode != ReactiveMode::WaveformSlice) {
        m_waveformHistory.clear();
        m_waveformFrameCounter = 0;
    }
    if (mode != ReactiveMode::Tunnel) {
        m_tunnelHistory.clear();
        m_tunnelHead = 0;
    }
}

void AudioReactiveEngine::setBlend(ReactiveBlend blend)   { m_blend = blend; }
void AudioReactiveEngine::setBaseFrame(const VoxelFrame& f) { m_baseFrame = f; }
void AudioReactiveEngine::setHueOffset(float hue)         { m_hueOffset = hue; }
void AudioReactiveEngine::setSensitivity(float s)         { m_sensitivity = std::max(0.0f, s); }

void AudioReactiveEngine::setWaveformSpeed(int speed) {
    speed = std::clamp(speed, 1, 30);
    // speed 30 → divider 1 (fastest, current default); speed 1 → divider 30 (slowest)
    m_waveformScrollDivider = 31 - speed;
    m_waveformFrameCounter  = 0;
}

int AudioReactiveEngine::waveformSpeed() const {
    return 31 - m_waveformScrollDivider;
}

void AudioReactiveEngine::setDecayPerFrame(float decay) {
    if (m_worker) m_worker->setDecayPerFrame(decay);
}

// ── Device enumeration ───────────────────────────────────────────────────────

QList<AudioReactiveEngine::DeviceInfo> AudioReactiveEngine::enumerateDevices() {
    QList<DeviceInfo> out;
    if (Pa_Initialize() != paNoError) return out;

    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels < 1) continue;

        const QString name = QString::fromUtf8(info->name);

        // Resolve the host API name (ALSA / PulseAudio / JACK / …) for the
        // device. Helps the user understand what kind of route they're picking.
        QString hostApi;
        if (const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(info->hostApi)) {
            hostApi = QString::fromUtf8(hostInfo->name);
        }

        // Monitor / loopback heuristic — PipeWire and PulseAudio expose the
        // system-output mix as a "*.monitor" / "Monitor of …" source.
        const bool isMonitor = name.contains(QStringLiteral(".monitor"),  Qt::CaseInsensitive)
                            || name.contains(QStringLiteral("monitor of"), Qt::CaseInsensitive)
                            || name.contains(QStringLiteral("loopback"),   Qt::CaseInsensitive);

        // Recommended-for-music-capture heuristic. These names route through
        // a sound server (PipeWire / PulseAudio / JACK) instead of an ALSA hw
        // device directly, so they're far more likely to capture useful audio
        // on a typical desktop. Hardware devices that genuinely have a
        // microphone or line-in still appear in the list — just not flagged.
        const bool recommended =
               isMonitor
            || name.compare(QStringLiteral("pipewire"), Qt::CaseInsensitive) == 0
            || name.compare(QStringLiteral("default"),  Qt::CaseInsensitive) == 0
            || name.compare(QStringLiteral("pulse"),    Qt::CaseInsensitive) == 0
            || hostApi.contains(QStringLiteral("PulseAudio"), Qt::CaseInsensitive)
            || hostApi.contains(QStringLiteral("JACK"),       Qt::CaseInsensitive);

        out.push_back({i, name, hostApi, isMonitor, recommended,
                       info->defaultSampleRate});
    }

    Pa_Terminate();

    // Sort: recommended first, then alphabetical by name within each group.
    std::sort(out.begin(), out.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
        if (a.recommended != b.recommended) return a.recommended;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return out;
}

// ── PulseAudio / PipeWire monitor source enumeration via pactl ──────────────

bool AudioReactiveEngine::isMonitorRoutingSupported() {
    QProcess p;
    p.start(QStringLiteral("pactl"), QStringList{QStringLiteral("--version")});
    if (!p.waitForFinished(500)) {
        p.kill();
        p.waitForFinished(200);
        return false;
    }
    return p.exitCode() == 0;
}

QList<AudioReactiveEngine::MonitorSource> AudioReactiveEngine::enumerateMonitors() {
    QList<MonitorSource> out;
    QProcess p;
    p.start(QStringLiteral("pactl"), QStringList{QStringLiteral("list"), QStringLiteral("sources")});
    if (!p.waitForFinished(2000)) {
        p.kill();
        p.waitForFinished(500);
        return out;
    }
    if (p.exitCode() != 0) return out;

    // pactl list sources output is a series of "Source #N\n    Name: ...\n
    // Description: ...\n    ..." blocks separated by blank lines.
    const QString blob = QString::fromUtf8(p.readAllStandardOutput());
    const QStringList lines = blob.split('\n');

    QString currentName;
    QString currentDesc;
    auto flush = [&]() {
        if (currentName.endsWith(QStringLiteral(".monitor"))) {
            out.push_back({currentName,
                           currentDesc.isEmpty() ? currentName : currentDesc});
        }
        currentName.clear();
        currentDesc.clear();
    };
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("Source #"))) {
            flush();
            continue;
        }
        if (line.startsWith(QStringLiteral("Name: "))) {
            currentName = line.mid(6);
        } else if (line.startsWith(QStringLiteral("Description: "))) {
            currentDesc = line.mid(13);
        }
    }
    flush();
    return out;
}

QList<int> AudioReactiveEngine::supportedSampleRates(int deviceIndex) {
    static constexpr int kCommonRates[] = {
        22050, 32000, 44100, 48000, 88200, 96000, 192000,
    };
    QList<int> out;
    if (Pa_Initialize() != paNoError) return out;

    const PaDeviceInfo* dev = Pa_GetDeviceInfo(deviceIndex);
    if (!dev) { Pa_Terminate(); return out; }

    PaStreamParameters params{};
    params.device                    = deviceIndex;
    params.channelCount              = 1;
    params.sampleFormat              = paFloat32;
    params.suggestedLatency          = dev->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    for (int rate : kCommonRates) {
        if (Pa_IsFormatSupported(&params, nullptr, static_cast<double>(rate))
                == paFormatIsSupported) {
            out.push_back(rate);
        }
    }
    Pa_Terminate();
    return out;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void AudioReactiveEngine::start(int deviceIndex, float sampleRate,
                                const QString& monitorSource) {
    if (m_running) return;

    m_thread = new QThread(this);
    m_worker = new AudioWorker(deviceIndex, sampleRate, monitorSource);
    if (m_mask) m_worker->setGridSize(m_mask->gridSize());
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &AudioWorker::start);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &AudioWorker::bandsReady,
            this, &AudioReactiveEngine::onBands, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::errorOccurred,
            this, &AudioReactiveEngine::errorOccurred, Qt::QueuedConnection);

    m_thread->start();
    m_running = true;
}

void AudioReactiveEngine::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        m_thread = nullptr;
        m_worker = nullptr;   // deleted via deleteLater when thread finished
    }
    m_pulse = {};
}

// ── Capture ──────────────────────────────────────────────────────────────────

void AudioReactiveEngine::startCapture(AnimationTimeline* timeline) {
    m_captureTarget = timeline;
    m_capturing     = (timeline != nullptr);
}

void AudioReactiveEngine::stopCapture() {
    m_capturing     = false;
    m_captureTarget = nullptr;
}

// ── Bands → frame dispatch (main thread) ─────────────────────────────────────

void AudioReactiveEngine::onBands(BandData data) {
    if (!m_running || m_mode == ReactiveMode::Off || !m_mask) return;

    // Apply user sensitivity to band magnitudes.
    for (auto& b : data.bands) b = std::clamp(b * m_sensitivity, 0.0f, 1.0f);

    VoxelFrame frame;
    switch (m_mode) {
        case ReactiveMode::EqBars:         frame = modeEqBars(data);         break;
        case ReactiveMode::BeatPulse:      frame = modeBeatPulse(data);      break;
        case ReactiveMode::WaveformSlice:  frame = modeWaveformSlice(data);  break;
        case ReactiveMode::SpectralColour: frame = modeSpectralColour(data); break;
        case ReactiveMode::RadialEq:       frame = modeRadialEq(data);       break;
        case ReactiveMode::Tunnel:         frame = modeTunnel(data);         break;
        case ReactiveMode::EnergyFloor:    frame = modeEnergyFloor(data);    break;
        case ReactiveMode::Off:            return;
    }

    frame = applyBlend(std::move(frame));
    emit frameReady(frame);
    if (data.beat) emit beatDetected();

    if (m_capturing && m_captureTarget) {
        emit frameCaptured(frame);
    }
}

// ── Mode implementations ─────────────────────────────────────────────────────

VoxelFrame AudioReactiveEngine::modeEqBars(const BandData& d) const {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();

    // For each (x, z) column, look up the band index that the column maps to
    // and light a number of LEDs proportional to that band's magnitude.
    // Band index from column: simple linear remap across the X axis.
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            const int bandIdx = std::min(n - 1, x * n / std::max(1, n));
            const float mag = d.bands[static_cast<size_t>(bandIdx)];
            const int height = static_cast<int>(std::round(mag * static_cast<float>(n)));
            const auto col = bandColor(bandIdx, n, mag, m_hueOffset);
            for (int y = 0; y < height; ++y) {
                if (m_mask->contains(x, y, z)) {
                    out.set(x, y, z, col[0], col[1], col[2]);
                }
            }
        }
    }
    return out;
}

VoxelFrame AudioReactiveEngine::modeBeatPulse(const BandData& d) {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();
    const float centre = (static_cast<float>(n) - 1.0f) * 0.5f;

    // Trigger a fresh pulse on each beat. The pulse expands a few units per
    // frame and fades; only one is active at a time (simple and readable).
    if (d.beat) {
        m_pulse.radius     = 0.0f;
        m_pulse.brightness = 1.0f;
        m_pulse.color      = hsvToRgb(360.0f * d.centroid + m_hueOffset, 1.0f, 1.0f);
        m_pulse.active     = true;
    }

    if (!m_pulse.active) return out;

    const float ringWidth = 0.9f;
    const float r2_lo = (m_pulse.radius - ringWidth) * (m_pulse.radius - ringWidth);
    const float r2_hi = (m_pulse.radius + ringWidth) * (m_pulse.radius + ringWidth);

    for (const auto& p : m_mask->positions()) {
        const float dx = static_cast<float>(p.x) - centre;
        const float dy = static_cast<float>(p.y) - centre;
        const float dz = static_cast<float>(p.z) - centre;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 >= r2_lo && d2 <= r2_hi) {
            const uint8_t r = clamp255(static_cast<int>(m_pulse.color[0] * m_pulse.brightness));
            const uint8_t g = clamp255(static_cast<int>(m_pulse.color[1] * m_pulse.brightness));
            const uint8_t b = clamp255(static_cast<int>(m_pulse.color[2] * m_pulse.brightness));
            out.set(p.x, p.y, p.z, r, g, b);
        }
    }

    m_pulse.radius     += 0.75f;
    m_pulse.brightness *= 0.85f;
    if (m_pulse.brightness < 0.05f || m_pulse.radius > static_cast<float>(n)) {
        m_pulse.active = false;
    }
    return out;
}

VoxelFrame AudioReactiveEngine::modeWaveformSlice(const BandData& d) {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();

    // Lazy (re)allocation. setMask() clears the history whenever the grid
    // size changes, so a size mismatch here means we just need fresh storage.
    if (static_cast<int>(m_waveformHistory.size()) != n) {
        m_waveformHistory.assign(static_cast<size_t>(n), std::vector<int>(n, -1));
    }

    // Compute the current row: for each x along the band axis, map band
    // magnitude → y height. -1 means "no voxel at this cell" so quiet
    // samples don't paint anything.
    std::vector<int> current(static_cast<size_t>(n), -1);
    for (int x = 0; x < n; ++x) {
        const int bandIdx = std::min(n - 1, x * n / std::max(1, n));
        const float mag   = d.bands[static_cast<size_t>(bandIdx)];
        if (mag > 0.001f) {
            current[static_cast<size_t>(x)] =
                std::clamp(static_cast<int>(std::round(mag * static_cast<float>(n - 1))),
                           0, n - 1);
        }
    }

    // Shift older rows toward z=0 (the back of the cube under the default
    // camera) only every m_waveformScrollDivider audio frames — that's how
    // setWaveformSpeed slows the scroll without freezing the live sample.
    // The newest row at z=n-1 is overwritten every frame regardless so the
    // user always sees the current audio.
    ++m_waveformFrameCounter;
    if (m_waveformFrameCounter >= m_waveformScrollDivider) {
        m_waveformFrameCounter = 0;
        for (int z = 0; z < n - 1; ++z) {
            m_waveformHistory[static_cast<size_t>(z)] =
                std::move(m_waveformHistory[static_cast<size_t>(z + 1)]);
        }
    }
    m_waveformHistory[static_cast<size_t>(n - 1)] = std::move(current);

    // Render the whole heightfield. Colour comes from the band index (x);
    // brightness scales with the height itself, so peaks look brighter than
    // troughs — gives the topographic-map feel the user asked for.
    for (int z = 0; z < n; ++z) {
        const auto& row = m_waveformHistory[static_cast<size_t>(z)];
        for (int x = 0; x < n; ++x) {
            const int y = row[static_cast<size_t>(x)];
            if (y < 0 || !m_mask->contains(x, y, z)) continue;

            const int   bandIdx = std::min(n - 1, x * n / std::max(1, n));
            const float yMag    = static_cast<float>(y)
                                  / static_cast<float>(std::max(1, n - 1));
            const auto  col     = bandColor(bandIdx, n, yMag, m_hueOffset);
            out.set(x, y, z, col[0], col[1], col[2]);
        }
    }
    return out;
}

VoxelFrame AudioReactiveEngine::modeSpectralColour(const BandData& d) const {
    VoxelFrame out;
    if (!m_mask) return out;

    const float v = std::clamp(d.rms * 6.0f, 0.0f, 1.0f);   // RMS is small; gain it
    const float h = std::fmod(360.0f * d.centroid + m_hueOffset, 360.0f);
    const auto  c = hsvToRgb(h, 1.0f, v);

    for (const auto& p : m_mask->positions()) {
        out.set(p.x, p.y, p.z, c[0], c[1], c[2]);
    }
    return out;
}

// ── RadialEq ─────────────────────────────────────────────────────────────────
//
// Bands as concentric shells from the cube centre. Distance-to-centre maps to
// band index — innermost shell = bass, outermost = treble. Light each voxel
// whose shell's band magnitude exceeds a threshold, scaled by the magnitude.

VoxelFrame AudioReactiveEngine::modeRadialEq(const BandData& d) const {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();
    const float centre  = (static_cast<float>(n) - 1.0f) * 0.5f;
    // Max possible distance is the cube's body diagonal half.
    const float maxDist = std::sqrt(3.0f) * centre + 0.5f;

    for (const auto& p : m_mask->positions()) {
        const float dx = static_cast<float>(p.x) - centre;
        const float dy = static_cast<float>(p.y) - centre;
        const float dz = static_cast<float>(p.z) - centre;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        const float t = std::clamp(dist / std::max(maxDist, 1e-6f), 0.0f, 0.9999f);
        const int   bandIdx = std::clamp(static_cast<int>(t * static_cast<float>(n)), 0, n - 1);
        const float mag     = d.bands[static_cast<size_t>(bandIdx)];

        if (mag < 0.05f) continue;
        const auto col = bandColor(bandIdx, n, mag, m_hueOffset);
        out.set(p.x, p.y, p.z, col[0], col[1], col[2]);
    }
    return out;
}

// ── Tunnel ───────────────────────────────────────────────────────────────────
//
// MilkDrop-style "flying through colour" effect. Each Z slice carries one past
// frame's full band snapshot rendered as an EQ wall (x = band index, y = bar
// height). New frame at Z=n-1 (front), older snapshots march back toward Z=0
// with brightness decaying with age.
//
// Memory: a circular buffer of N std::array<float,32> snapshots — trivial.

VoxelFrame AudioReactiveEngine::modeTunnel(const BandData& d) {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();

    // (Re)allocate the history if grid size changed since the last frame.
    if (static_cast<int>(m_tunnelHistory.size()) != n) {
        m_tunnelHistory.assign(static_cast<size_t>(n), std::array<float, 32>{});
        m_tunnelHead = 0;
    }

    // Write the current snapshot at the head; advance head modulo n.
    m_tunnelHistory[static_cast<size_t>(m_tunnelHead)] = d.bands;
    m_tunnelHead = (m_tunnelHead + 1) % n;

    // Render each Z slice from the corresponding history entry. Z=n-1 is the
    // most-recently-written slot (head - 1); Z=0 is the oldest (head). Age
    // fades brightness so the back of the tunnel dims naturally.
    for (int z = 0; z < n; ++z) {
        const int ageFromNewest = (n - 1) - z;     // 0 at front, n-1 at back
        const int idx = ((m_tunnelHead - 1 - ageFromNewest) % n + n) % n;
        const auto& bands = m_tunnelHistory[static_cast<size_t>(idx)];

        const float ageFade = 1.0f - static_cast<float>(ageFromNewest)
                                       / static_cast<float>(std::max(1, n - 1));

        for (int x = 0; x < n; ++x) {
            const int   bandIdx = std::min(n - 1, x);
            const float mag     = bands[static_cast<size_t>(bandIdx)];
            if (mag < 0.05f) continue;

            const int  height = static_cast<int>(std::round(mag * static_cast<float>(n)));
            const auto col    = bandColor(bandIdx, n, mag * ageFade, m_hueOffset);
            for (int y = 0; y < std::min(height, n); ++y) {
                if (m_mask->contains(x, y, z)) {
                    out.set(x, y, z, col[0], col[1], col[2]);
                }
            }
        }
    }
    return out;
}

// ── EnergyFloor ──────────────────────────────────────────────────────────────
//
// A "wall of light" rising from Y=0 uniformly across Z, with the X axis
// coloured by frequency band. Wall height is driven by overall RMS so it
// reads as a single visual response to total volume, while the colour pattern
// still varies with the spectrum. Visual contrast against EqBars: that mode
// has per-column heights (jagged ceiling); this one has a flat ceiling that
// rises and falls with RMS.

VoxelFrame AudioReactiveEngine::modeEnergyFloor(const BandData& d) const {
    VoxelFrame out;
    if (!m_mask) return out;
    const int n = m_mask->gridSize();

    // RMS-driven ceiling. RMS is small in practice; gain it to fill the cube.
    const int ceiling = std::clamp(
        static_cast<int>(std::round(d.rms * 8.0f * static_cast<float>(n))),
        1, n);

    for (int x = 0; x < n; ++x) {
        const int   bandIdx = std::min(n - 1, x);
        const float mag     = d.bands[static_cast<size_t>(bandIdx)];
        if (mag < 0.05f) continue;

        const auto col = bandColor(bandIdx, n, mag, m_hueOffset);
        for (int y = 0; y < ceiling; ++y) {
            // Brightness falls off with height inside the wall — bottom is
            // intense, top of the wall is faded to its tip.
            const float fade = 1.0f - static_cast<float>(y)
                                       / static_cast<float>(std::max(1, ceiling));
            const uint8_t r = static_cast<uint8_t>(std::round(col[0] * fade));
            const uint8_t g = static_cast<uint8_t>(std::round(col[1] * fade));
            const uint8_t b = static_cast<uint8_t>(std::round(col[2] * fade));

            for (int z = 0; z < n; ++z) {
                if (m_mask->contains(x, y, z)) {
                    out.set(x, y, z, r, g, b);
                }
            }
        }
    }
    return out;
}

// ── Blend ────────────────────────────────────────────────────────────────────

VoxelFrame AudioReactiveEngine::applyBlend(VoxelFrame reactive) const {
    if (m_blend == ReactiveBlend::Replace || m_baseFrame.empty()) return reactive;

    VoxelFrame out = m_baseFrame;
    if (m_blend == ReactiveBlend::Additive) {
        for (const auto& [k, c] : reactive.voxels()) {
            auto base = out.get(k.x, k.y, k.z);
            if (base) {
                out.set(k.x, k.y, k.z,
                        clamp255(c[0] + (*base)[0]),
                        clamp255(c[1] + (*base)[1]),
                        clamp255(c[2] + (*base)[2]));
            } else {
                out.set(k.x, k.y, k.z, c[0], c[1], c[2]);
            }
        }
    } else {   // Modulate
        // Reactive amplitude (luma of the reactive pixel) multiplies base brightness.
        // Where reactive has no pixel, base passes through unchanged.
        for (const auto& [k, baseC] : m_baseFrame.voxels()) {
            auto react = reactive.get(k.x, k.y, k.z);
            if (!react) continue;
            const float lum = (0.299f * (*react)[0] + 0.587f * (*react)[1] + 0.114f * (*react)[2])
                              / 255.0f;
            out.set(k.x, k.y, k.z,
                    clamp255(static_cast<int>(baseC[0] * lum)),
                    clamp255(static_cast<int>(baseC[1] * lum)),
                    clamp255(static_cast<int>(baseC[2] * lum)));
        }
    }
    return out;
}

// ── Colour helpers ───────────────────────────────────────────────────────────

std::array<uint8_t, 3> AudioReactiveEngine::hsvToRgb(float h, float s, float v) {
    h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f);
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if      (h <  60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }
    return {
        clamp255(static_cast<int>(std::round((r + m) * 255.0f))),
        clamp255(static_cast<int>(std::round((g + m) * 255.0f))),
        clamp255(static_cast<int>(std::round((b + m) * 255.0f))),
    };
}

std::array<uint8_t, 3> AudioReactiveEngine::bandColor(int bandIdx, int totalBands,
                                                      float magnitude, float hueOffset) {
    const float hue = (totalBands > 1)
                          ? 240.0f * (1.0f - static_cast<float>(bandIdx)
                                             / static_cast<float>(totalBands - 1))
                          : 0.0f;
    return hsvToRgb(hue + hueOffset, 1.0f, std::clamp(magnitude, 0.0f, 1.0f));
}
