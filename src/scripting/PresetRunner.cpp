#include "PresetRunner.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

// ── Python interpreter detection ─────────────────────────────────────────────
//
// The presets all depend on numpy. The user's `/usr/bin/python3` may or may
// not have it (Ubuntu's python3-numpy package is not always installed); a
// conda or miniconda Python typically does. Probe a short list of likely
// candidates and cache the first one that successfully imports numpy.
//
// Cached across PresetRunner instances within the same Scintilla session.

QString g_pythonCache;

QString probePythonInterpreter() {
    if (!g_pythonCache.isEmpty()) return g_pythonCache;

    // Candidate order: PATH `python3` first (covers system + apt python3-numpy),
    // then common conda / miniforge install paths in the user's home.
    QStringList candidates{ QStringLiteral("python3") };
    const QString home = QDir::homePath();
    for (const QString& base : {
             QStringLiteral("/miniconda3/bin/python3"),
             QStringLiteral("/anaconda3/bin/python3"),
             QStringLiteral("/miniforge3/bin/python3"),
             QStringLiteral("/mambaforge/bin/python3"),
         }) {
        candidates << (home + base);
    }
    candidates << QStringLiteral("/usr/bin/python3");

    for (const QString& exe : candidates) {
        QProcess test;
        test.start(exe, QStringList{QStringLiteral("-c"),
                                    QStringLiteral("import numpy")});
        if (!test.waitForFinished(2000)) {
            test.kill();
            test.waitForFinished(200);
            continue;
        }
        if (test.exitCode() == 0) {
            g_pythonCache = exe;
            qDebug() << "[preset] Python interpreter with numpy:" << exe;
            return exe;
        }
    }
    return QString();   // none found — caller handles the error
}

// Resolve the runtime presets directory so we can set PYTHONPATH for the
// subprocess. Looks alongside the executable first, then walks up to find
// the source-tree presets/ (useful during development).
QString locatePresetsRoot() {
    const QStringList places{
        QCoreApplication::applicationDirPath() + QStringLiteral("/presets"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../presets"),
        QDir::currentPath() + QStringLiteral("/presets"),
    };
    for (const QString& p : places) {
        if (QFileInfo(p + QStringLiteral("/led_cube/__init__.py")).exists()) {
            return QDir(p).absolutePath();
        }
    }
    return QString();
}

}  // namespace

// ── Construction / teardown ──────────────────────────────────────────────────

PresetRunner::PresetRunner(QObject* parent) : QObject(parent) {}

PresetRunner::~PresetRunner() {
    stopSubprocess();
}

// ── Public API ───────────────────────────────────────────────────────────────

void PresetRunner::loadPreset(const QString& filePath) {
    if (!QFileInfo(filePath).exists()) {
        emit errorOccurred(tr("Preset file does not exist: %1").arg(filePath), -1);
        return;
    }

    // Hot-reload semantics: tearing down and restarting the subprocess is the
    // safest way to drop any state the old preset accumulated, even if the
    // file path is the same.
    stopSubprocess();

    m_filePath   = filePath;
    m_presetName = QFileInfo(filePath).baseName();
    m_readBuf.clear();

    startSubprocess();

    // Watch for file changes so the preset hot-reloads on save (DEC-020).
    if (!m_watcher) {
        m_watcher = std::make_unique<QFileSystemWatcher>(this);
        connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
                this, &PresetRunner::onFileChanged);
    }
    // Remove anything we were previously watching, then add the new path.
    if (!m_watcher->files().isEmpty()) m_watcher->removePaths(m_watcher->files());
    m_watcher->addPath(filePath);
}

void PresetRunner::unload() {
    stopSubprocess();
    m_filePath.clear();
    m_presetName.clear();
    if (m_watcher && !m_watcher->files().isEmpty()) {
        m_watcher->removePaths(m_watcher->files());
    }
    emit presetUnloaded();
}

void PresetRunner::pushFrame(const BandData& audio) {
    if (!isLoaded()) return;

    QJsonArray bands;
    // Only the active bands matter; the rest are zero-initialised in BandData.
    // Use gridSize for the live count.
    const int n = std::min(m_gridSize, static_cast<int>(audio.bands.size()));
    for (int i = 0; i < n; ++i) bands.append(static_cast<double>(audio.bands[i]));

    QJsonObject audioObj{
        {"bands",    bands},
        {"rms",      static_cast<double>(audio.rms)},
        {"centroid", static_cast<double>(audio.centroid)},
        {"beat",     audio.beat},
    };
    QJsonObject root{
        {"type",  "frame"},
        {"audio", audioObj},
    };
    writeLine(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PresetRunner::setCubeMeta(int gridSize, const QString& shape, int ledCount) {
    m_gridSize = gridSize;
    m_shape    = shape;
    m_ledCount = ledCount;
    // If the preset is already running, re-send the load so it sees the new
    // cube. This effectively restarts the preset's on_load with the new mask.
    if (isLoaded()) sendLoad();
}

// ── Subprocess lifecycle ─────────────────────────────────────────────────────

void PresetRunner::startSubprocess() {
    const QString python = probePythonInterpreter();
    if (python.isEmpty()) {
        emit errorOccurred(
            tr("No Python interpreter with numpy was found. Install one with "
               "`sudo apt install python3-numpy` or `conda install numpy`."), -1);
        return;
    }

    const QString presetsRoot = locatePresetsRoot();
    if (presetsRoot.isEmpty()) {
        emit errorOccurred(
            tr("Could not locate the presets/ directory next to the binary."), -1);
        return;
    }

    m_process = std::make_unique<QProcess>(this);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString existing = env.value(QStringLiteral("PYTHONPATH"));
    env.insert(QStringLiteral("PYTHONPATH"),
               existing.isEmpty()
                   ? presetsRoot
                   : presetsRoot + QDir::listSeparator() + existing);
    // Force unbuffered stdio on the Python side so the JSON-line protocol
    // doesn't block in pipe buffers.
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    m_process->setProcessEnvironment(env);

    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &PresetRunner::onProcessOutput);
    connect(m_process.get(), &QProcess::readyReadStandardError,
            this, &PresetRunner::onProcessError);

    const QStringList args{
        QStringLiteral("-u"),                    // unbuffered, belt-and-braces
        QStringLiteral("-m"), QStringLiteral("led_cube._runner"),
        m_filePath,
    };
    qDebug().noquote() << "[preset] launching" << python << args
                       << "  PYTHONPATH=" << presetsRoot;
    m_process->start(python, args);

    if (!m_process->waitForStarted(3000)) {
        emit errorOccurred(tr("Failed to start preset subprocess"), -1);
        m_process.reset();
        return;
    }

    // Send the initial load message so the preset's on_load can initialise.
    sendLoad();
}

void PresetRunner::stopSubprocess() {
    if (!m_process) return;
    if (m_process->state() == QProcess::Running) {
        writeLine(QStringLiteral("{\"type\":\"unload\"}").toUtf8());
        if (!m_process->waitForFinished(500)) {
            m_process->terminate();
            if (!m_process->waitForFinished(500)) m_process->kill();
        }
    }
    m_process.reset();
    m_readBuf.clear();
}

void PresetRunner::sendLoad() {
    QJsonObject cubeMeta{
        {"grid_size", m_gridSize},
        {"shape",     m_shape},
        // positions intentionally omitted in this iteration — the runner
        // falls back to the full N³ cube grid. Real mask positions will
        // flow through in step C alongside audio integration.
    };
    QJsonObject root{
        {"type", "load"},
        {"cube", cubeMeta},
    };
    writeLine(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PresetRunner::writeLine(const QByteArray& json) {
    if (!m_process || m_process->state() != QProcess::Running) return;
    m_process->write(json);
    m_process->write("\n");
}

// ── Output handling ──────────────────────────────────────────────────────────

void PresetRunner::onProcessOutput() {
    if (!m_process) return;
    m_readBuf.append(m_process->readAllStandardOutput());

    // One JSON message per newline. The runner flushes after every write so
    // we should see whole lines arrive promptly.
    while (true) {
        const int nl = m_readBuf.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_readBuf.left(nl);
        m_readBuf.remove(0, nl + 1);
        if (line.trimmed().isEmpty()) continue;

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "[preset] bad JSON from subprocess:" << pe.errorString()
                       << "raw:" << line;
            continue;
        }

        const QJsonObject obj = doc.object();
        const QString     type = obj.value(QStringLiteral("type")).toString();

        if (type == QLatin1String("ready")) {
            m_presetName = obj.value(QStringLiteral("name")).toString(m_presetName);
            qDebug() << "[preset] ready —" << m_presetName;
            emit presetLoaded(m_presetName);
        }
        else if (type == QLatin1String("frame")) {
            VoxelFrame frame;
            const QJsonObject voxels = obj.value(QStringLiteral("voxels")).toObject();
            for (auto it = voxels.constBegin(); it != voxels.constEnd(); ++it) {
                const QStringList parts = it.key().split(',');
                if (parts.size() != 3) continue;
                bool ok1, ok2, ok3;
                const int x = parts[0].toInt(&ok1);
                const int y = parts[1].toInt(&ok2);
                const int z = parts[2].toInt(&ok3);
                if (!ok1 || !ok2 || !ok3) continue;
                const QJsonArray rgb = it.value().toArray();
                if (rgb.size() != 3) continue;
                frame.set(x, y, z,
                          static_cast<uint8_t>(std::clamp(rgb[0].toInt(), 0, 255)),
                          static_cast<uint8_t>(std::clamp(rgb[1].toInt(), 0, 255)),
                          static_cast<uint8_t>(std::clamp(rgb[2].toInt(), 0, 255)));
            }
            emit frameReady(std::move(frame));
        }
        else if (type == QLatin1String("play")) {
            const int fps = obj.value(QStringLiteral("fps")).toInt(12);
            qDebug() << "[preset] animation complete — fps:" << fps;
            emit animationComplete(fps);
        }
        else if (type == QLatin1String("error")) {
            const QString msg  = obj.value(QStringLiteral("message")).toString();
            const int     line = obj.value(QStringLiteral("line")).toInt(-1);
            qWarning().noquote() << "[preset] error:" << msg;
            emit errorOccurred(msg, line);
        }
        else {
            qWarning() << "[preset] unknown message type:" << type;
        }
    }
}

void PresetRunner::onProcessError() {
    if (!m_process) return;
    const QByteArray err = m_process->readAllStandardError();
    if (err.isEmpty()) return;
    // Surface Python's traceback / import errors to the user — typically
    // numpy missing or a syntax error in the preset file.
    qWarning().noquote() << "[preset] subprocess stderr:" << QString::fromUtf8(err).trimmed();
    emit errorOccurred(QString::fromUtf8(err).trimmed(), -1);
}

void PresetRunner::onFileChanged(const QString& path) {
    qDebug() << "[preset] file changed, hot-reloading:" << path;
    // Some editors save by writing to a temp file then renaming, which can
    // remove our watcher's hook. Re-load resubscribes the watcher.
    loadPreset(path);
    emit hotReloaded();
}
