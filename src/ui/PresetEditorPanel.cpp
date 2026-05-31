#include "PresetEditorPanel.h"

#include "PythonHighlighter.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTextStream>
#include <QVBoxLayout>

PresetEditorPanel::PresetEditorPanel(QWidget* parent) : QWidget(parent) {
    buildLayout();
    refreshHeader();
}

void PresetEditorPanel::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Header row ───────────────────────────────────────────────────────────
    auto* header = new QHBoxLayout();
    m_pathLabel = new QLabel(this);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(m_pathLabel, 1);

    m_cubeLabel = new QLabel(this);
    m_cubeLabel->setStyleSheet(QStringLiteral("color:#888;"));
    m_cubeLabel->setToolTip(
        tr("Current cube shape and grid size. Update via Shape menu, or "
           "declare grid_size / shape attributes in the script."));
    header->addWidget(m_cubeLabel);

    m_runButton = new QPushButton(tr("Run"), this);
    m_runButton->setToolTip(
        tr("Run the script as an animation. The timeline is cleared and "
           "filled with the frames the script emits via cube.frame()."));
    connect(m_runButton, &QPushButton::clicked, this, [this]() {
        if (!m_path.isEmpty()) emit runRequested(m_path);
    });
    header->addWidget(m_runButton);

    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setShortcut(QKeySequence::Save);
    m_saveButton->setToolTip(tr("Save the preset back to disk. The runner's "
                                "file watcher will hot-reload the subprocess."));
    connect(m_saveButton, &QPushButton::clicked, this, &PresetEditorPanel::save);
    header->addWidget(m_saveButton);

    root->addLayout(header);

    // ── Editor ───────────────────────────────────────────────────────────────
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setTabChangesFocus(false);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setPlaceholderText(
        tr("(no preset loaded — pick one via the Audio reactive panel's "
           "Load preset… button)"));
    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &PresetEditorPanel::onTextChanged);
    root->addWidget(m_editor, 1);

    m_highlighter = new PythonHighlighter(m_editor->document());

    // Disabled until a preset is loaded.
    m_editor->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_runButton->setEnabled(false);
}

void PresetEditorPanel::loadFile(const QString& path) {
    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        emit saveFailed(path, tr("Could not open: %1").arg(f.errorString()));
        return;
    }
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    // If the path and content match what's already loaded, treat as a no-op.
    // This handles the self-trigger case: when our own save() writes the file,
    // QFileSystemWatcher fires and the runner re-emits presetLoaded, which
    // would otherwise reload the buffer mid-edit.
    if (path == m_path && content == m_editor->toPlainText()) {
        setDirty(false);
        return;
    }

    m_path = path;
    m_suppressDirty = true;
    m_editor->setPlainText(content);
    m_suppressDirty = false;
    setDirty(false);
    m_editor->setEnabled(true);
    m_saveButton->setEnabled(true);
    m_runButton->setEnabled(true);
    refreshHeader();
}

void PresetEditorPanel::clearFile() {
    m_path.clear();
    m_suppressDirty = true;
    m_editor->clear();
    m_suppressDirty = false;
    setDirty(false);
    m_editor->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_runButton->setEnabled(false);
    refreshHeader();
}

void PresetEditorPanel::setCubeInfo(int gridSize, const QString& shape, int ledCount) {
    if (!m_cubeLabel) return;
    m_cubeLabel->setText(tr("Cube %1³ %2  ·  %3 LEDs")
        .arg(gridSize)
        .arg(shape)
        .arg(ledCount));
}

void PresetEditorPanel::save() {
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (!f.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        emit saveFailed(m_path, tr("Could not write: %1").arg(f.errorString()));
        return;
    }
    {
        QTextStream out(&f);
        out << m_editor->toPlainText();
    }
    f.close();
    setDirty(false);
    emit fileSaved(m_path);
}

void PresetEditorPanel::onTextChanged() {
    if (m_suppressDirty) return;
    setDirty(true);
}

void PresetEditorPanel::setDirty(bool d) {
    if (m_dirty == d) return;
    m_dirty = d;
    refreshHeader();
}

void PresetEditorPanel::refreshHeader() {
    const QString name = m_path.isEmpty()
        ? tr("(no preset loaded)")
        : QFileInfo(m_path).fileName();
    m_pathLabel->setText(m_dirty ? tr("● %1").arg(name) : name);
    m_pathLabel->setToolTip(m_path);
}
