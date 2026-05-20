#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>
#include "audio/AudioReactiveEngine.h"
#include "core/VoxelFrame.h"

// ── PresetRunner ──────────────────────────────────────────────────────────────
//
// Manages the Python subprocess that runs the active preset.
// Feeds AudioSnapshot to the subprocess each frame, receives VoxelFrame back.
// Watches the preset file for changes and hot-reloads (DEC-020).
//
// Protocol (stdin/stdout JSON, one object per line):
//   Host → Subprocess:  {"type":"frame",  "audio":{...}}
//   Host → Subprocess:  {"type":"load",   "cube":{...}}
//   Host → Subprocess:  {"type":"unload"}
//   Subprocess → Host:  {"type":"frame",  "voxels":{...}}
//   Subprocess → Host:  {"type":"error",  "message":"...", "line":N}
//   Subprocess → Host:  {"type":"ready"}

class PresetRunner : public QObject {
    Q_OBJECT

public:
    explicit PresetRunner(QObject* parent = nullptr);
    ~PresetRunner() override;

    // Load a preset file. Starts subprocess if not running.
    void loadPreset(const QString& filePath);

    // Unload current preset and stop subprocess.
    void unload();

    // Push an audio frame. Returns immediately; frameReady() fires asynchronously.
    void pushFrame(const BandData& audio);

    // Update cube metadata (call after shape/size change).
    void setCubeMeta(int gridSize, const QString& shape, int ledCount);

    [[nodiscard]] bool isLoaded()    const { return m_process && m_process->state() == QProcess::Running; }
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString presetName() const { return m_presetName; }

signals:
    void frameReady(VoxelFrame frame);
    void errorOccurred(QString message, int line);   // line = -1 if unknown
    void presetLoaded(QString name);
    void presetUnloaded();
    void hotReloaded();

private slots:
    void onProcessOutput();
    void onProcessError();
    void onFileChanged(const QString& path);

private:
    void startSubprocess();
    void stopSubprocess();
    void sendLoad();
    void writeLine(const QByteArray& json);

    QString   m_filePath;
    QString   m_presetName;
    int       m_gridSize  = 8;
    QString   m_shape     = "cube";
    int       m_ledCount  = 512;

    std::unique_ptr<QProcess>          m_process;
    std::unique_ptr<QFileSystemWatcher> m_watcher;

    QByteArray m_readBuf;   // partial line accumulator
};
