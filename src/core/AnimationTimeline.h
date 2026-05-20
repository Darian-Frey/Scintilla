#pragma once

#include <QObject>
#include <QTimer>
#include <vector>
#include "VoxelFrame.h"

// ── PlaybackMode ──────────────────────────────────────────────────────────────
enum class PlaybackMode {
    PlayOnce,
    Loop,
    PingPong,
};

// ── AnimationTimeline ─────────────────────────────────────────────────────────
//
// Ordered sequence of VoxelFrame plus playback state. Always contains at least
// one frame — clearAll() resets to a single empty frame rather than producing
// an empty timeline.
//
// Frame mutations (add/duplicate/delete/move) emit timelineStructureChanged.
// Editing voxels in the current frame is done via currentFrame() and should be
// followed by a frameContentChanged emission (the caller signals after each
// batch of edits to avoid one emission per voxel).

class AnimationTimeline : public QObject {
    Q_OBJECT

public:
    explicit AnimationTimeline(QObject* parent = nullptr);

    // ── Frame access ──────────────────────────────────────────────────────────
    [[nodiscard]] int frameCount()   const { return static_cast<int>(m_frames.size()); }
    [[nodiscard]] int currentIndex() const { return m_current; }

    [[nodiscard]] VoxelFrame&       currentFrame();
    [[nodiscard]] const VoxelFrame& currentFrame() const;

    [[nodiscard]] VoxelFrame&       frameAt(int i);
    [[nodiscard]] const VoxelFrame& frameAt(int i) const;

    // ── Frame editing ─────────────────────────────────────────────────────────
    void addFrame();             // append empty, select it
    void duplicateCurrent();     // copy current, insert after, select copy
    void deleteCurrent();        // delete; collapse to 1 empty if last
    void selectFrame(int i);
    void moveFrame(int from, int to);

    // Reset to a single empty frame. Used after a shape change (DEC-006).
    void clearAll();

    // Replace the entire frame sequence (used by JSON load).
    void replaceFrames(std::vector<VoxelFrame> frames, int currentIndex);

    // True if any frame has at least one lit voxel.
    [[nodiscard]] bool hasContent() const;

    // Signal that the current frame's voxels have been edited.
    // Callers invoke this after a batch of set/erase to coalesce updates.
    void notifyCurrentFrameEdited();

    // ── Playback ──────────────────────────────────────────────────────────────
    void play();
    void stop();
    [[nodiscard]] bool isPlaying() const { return m_playing; }

    void setFps(int fps);                                 // clamped to [1, 60]
    [[nodiscard]] int fps() const { return m_fps; }

    void setPlaybackMode(PlaybackMode m)                  { m_mode = m; }
    [[nodiscard]] PlaybackMode playbackMode() const       { return m_mode; }

signals:
    void currentFrameChanged(int index);
    void frameContentChanged(int index);
    void timelineStructureChanged();
    void playbackStateChanged(bool playing);

private slots:
    void advanceFrame();

private:
    void emitCurrentChanged();
    void restartTimerIfPlaying();

    std::vector<VoxelFrame> m_frames;
    int          m_current   = 0;
    int          m_fps       = 12;
    PlaybackMode m_mode      = PlaybackMode::Loop;
    bool         m_playing   = false;
    int          m_direction = 1;          // +1 / −1, used by PingPong

    QTimer m_timer;
};
