#pragma once

#include <QWidget>

class QPlainTextEdit;
class QLabel;
class QPushButton;
class PythonHighlighter;

// ── PresetEditorPanel ───────────────────────────────────────────────────────
//
// In-app text editor for the currently-loaded Python preset. Source is loaded
// from disk via loadFile() — typically wired to PresetRunner::presetLoaded —
// and saved back through Ctrl+S or the Save button. The PresetRunner's
// QFileSystemWatcher picks up the change and hot-reloads the subprocess
// (Phase 4 step D).

class PresetEditorPanel : public QWidget {
    Q_OBJECT

public:
    explicit PresetEditorPanel(QWidget* parent = nullptr);

    void loadFile(const QString& path);
    void clearFile();

    [[nodiscard]] QString filePath() const { return m_path; }
    [[nodiscard]] bool    isDirty()  const { return m_dirty;  }

public slots:
    void save();

signals:
    void fileSaved(const QString& path);
    void saveFailed(const QString& path, const QString& reason);

    // Emitted when the user clicks "Run" — MainWindow runs the file as an
    // animation script (it auto-detects Preset vs Animation; if a reactive
    // preset is loaded this surfaces a clear error from the runner).
    void runRequested(const QString& path);

private slots:
    void onTextChanged();

private:
    void buildLayout();
    void setDirty(bool d);
    void refreshHeader();

    QPlainTextEdit*    m_editor       = nullptr;
    QLabel*            m_pathLabel    = nullptr;
    QPushButton*       m_saveButton   = nullptr;
    QPushButton*       m_runButton    = nullptr;
    PythonHighlighter* m_highlighter  = nullptr;

    QString m_path;
    bool    m_dirty         = false;
    bool    m_suppressDirty = false;   // true while loadFile() is mutating the buffer
};
