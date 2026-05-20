#include "AnimationTimeline.h"

#include <algorithm>

AnimationTimeline::AnimationTimeline(QObject* parent)
    : QObject(parent) {
    m_frames.emplace_back();   // always at least one frame
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &AnimationTimeline::advanceFrame);
}

// ── Frame access ──────────────────────────────────────────────────────────────

VoxelFrame& AnimationTimeline::currentFrame() {
    return m_frames[static_cast<size_t>(m_current)];
}

const VoxelFrame& AnimationTimeline::currentFrame() const {
    return m_frames[static_cast<size_t>(m_current)];
}

VoxelFrame& AnimationTimeline::frameAt(int i) {
    return m_frames.at(static_cast<size_t>(i));
}

const VoxelFrame& AnimationTimeline::frameAt(int i) const {
    return m_frames.at(static_cast<size_t>(i));
}

// ── Frame editing ─────────────────────────────────────────────────────────────

void AnimationTimeline::addFrame() {
    m_frames.emplace_back();
    m_current = static_cast<int>(m_frames.size()) - 1;
    emit timelineStructureChanged();
    emitCurrentChanged();
}

void AnimationTimeline::duplicateCurrent() {
    m_frames.insert(m_frames.begin() + m_current + 1, m_frames[m_current]);
    m_current += 1;
    emit timelineStructureChanged();
    emitCurrentChanged();
}

void AnimationTimeline::deleteCurrent() {
    if (m_frames.size() <= 1) {
        // Collapse to a single empty frame rather than producing an empty timeline.
        m_frames[0].clear();
        m_current = 0;
        emit timelineStructureChanged();
        emit frameContentChanged(0);
        emitCurrentChanged();
        return;
    }
    m_frames.erase(m_frames.begin() + m_current);
    if (m_current >= static_cast<int>(m_frames.size())) {
        m_current = static_cast<int>(m_frames.size()) - 1;
    }
    emit timelineStructureChanged();
    emitCurrentChanged();
}

void AnimationTimeline::selectFrame(int i) {
    if (i < 0 || i >= static_cast<int>(m_frames.size()) || i == m_current) return;
    m_current = i;
    emitCurrentChanged();
}

void AnimationTimeline::moveFrame(int from, int to) {
    const int n = static_cast<int>(m_frames.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;

    VoxelFrame moved = std::move(m_frames[from]);
    m_frames.erase(m_frames.begin() + from);
    m_frames.insert(m_frames.begin() + to, std::move(moved));

    if (m_current == from)                m_current = to;
    else if (from < m_current && to >= m_current) m_current -= 1;
    else if (from > m_current && to <= m_current) m_current += 1;

    emit timelineStructureChanged();
    emitCurrentChanged();
}

void AnimationTimeline::clearAll() {
    m_frames.clear();
    m_frames.emplace_back();
    m_current   = 0;
    m_direction = 1;
    emit timelineStructureChanged();
    emit frameContentChanged(0);
    emitCurrentChanged();
}

void AnimationTimeline::replaceFrames(std::vector<VoxelFrame> frames, int currentIndex) {
    if (frames.empty()) frames.emplace_back();
    m_frames    = std::move(frames);
    m_current   = std::clamp(currentIndex, 0, static_cast<int>(m_frames.size()) - 1);
    m_direction = 1;
    emit timelineStructureChanged();
    emitCurrentChanged();
}

bool AnimationTimeline::hasContent() const {
    return std::any_of(m_frames.begin(), m_frames.end(),
                       [](const VoxelFrame& f) { return !f.empty(); });
}

void AnimationTimeline::notifyCurrentFrameEdited() {
    emit frameContentChanged(m_current);
}

// ── Playback ──────────────────────────────────────────────────────────────────

void AnimationTimeline::play() {
    if (m_playing || m_frames.size() <= 1) return;
    m_playing   = true;
    m_direction = 1;
    m_timer.start(1000 / std::max(1, m_fps));
    emit playbackStateChanged(true);
}

void AnimationTimeline::stop() {
    if (!m_playing) return;
    m_playing = false;
    m_timer.stop();
    emit playbackStateChanged(false);
}

void AnimationTimeline::setFps(int fps) {
    m_fps = std::clamp(fps, 1, 60);
    restartTimerIfPlaying();
}

void AnimationTimeline::advanceFrame() {
    const int n = static_cast<int>(m_frames.size());
    if (n <= 1) { stop(); return; }

    switch (m_mode) {
        case PlaybackMode::PlayOnce:
            if (m_current >= n - 1) { stop(); return; }
            m_current += 1;
            break;

        case PlaybackMode::Loop:
            m_current = (m_current + 1) % n;
            break;

        case PlaybackMode::PingPong:
            m_current += m_direction;
            if (m_current >= n - 1)      { m_current = n - 1; m_direction = -1; }
            else if (m_current <= 0)     { m_current = 0;     m_direction = +1; }
            break;
    }
    emitCurrentChanged();
}

void AnimationTimeline::emitCurrentChanged() {
    emit currentFrameChanged(m_current);
}

void AnimationTimeline::restartTimerIfPlaying() {
    if (!m_playing) return;
    m_timer.stop();
    m_timer.start(1000 / std::max(1, m_fps));
}
