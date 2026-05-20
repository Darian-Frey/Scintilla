#include "JsonSerializer.h"

#include "AnimationTimeline.h"
#include "VoxelFrame.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <algorithm>

namespace {

QString keyFor(int x, int y, int z) {
    return QStringLiteral("%1,%2,%3").arg(x).arg(y).arg(z);
}

bool parseKey(const QString& s, int& x, int& y, int& z) {
    const auto parts = s.split(',');
    if (parts.size() != 3) return false;
    bool a, b, c;
    x = parts[0].toInt(&a);
    y = parts[1].toInt(&b);
    z = parts[2].toInt(&c);
    return a && b && c;
}

QJsonObject frameToJson(const VoxelFrame& f, double duration) {
    QJsonObject voxels;
    for (const auto& [k, color] : f.voxels()) {
        QJsonArray rgb;
        rgb.append(color[0]); rgb.append(color[1]); rgb.append(color[2]);
        voxels[keyFor(k.x, k.y, k.z)] = rgb;
    }
    QJsonObject obj;
    obj["duration"] = duration;
    obj["voxels"]   = voxels;
    return obj;
}

bool readFrame(const QJsonObject& obj, VoxelFrame& out, QString* errorOut) {
    const QJsonValue v = obj.value("voxels");
    if (!v.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("frame missing 'voxels' object");
        return false;
    }
    const QJsonObject voxels = v.toObject();
    for (auto it = voxels.constBegin(); it != voxels.constEnd(); ++it) {
        int x, y, z;
        if (!parseKey(it.key(), x, y, z)) {
            if (errorOut) *errorOut = QStringLiteral("malformed voxel key: %1").arg(it.key());
            return false;
        }
        const QJsonArray rgb = it.value().toArray();
        if (rgb.size() != 3) {
            if (errorOut) *errorOut = QStringLiteral("voxel %1 has %2 values, expected 3").arg(it.key()).arg(rgb.size());
            return false;
        }
        out.set(x, y, z,
                static_cast<uint8_t>(std::clamp(rgb[0].toInt(), 0, 255)),
                static_cast<uint8_t>(std::clamp(rgb[1].toInt(), 0, 255)),
                static_cast<uint8_t>(std::clamp(rgb[2].toInt(), 0, 255)));
    }
    return true;
}

}  // namespace

// ── Save ─────────────────────────────────────────────────────────────────────

bool JsonSerializer::save(const QString& path,
                          const ShapeMask& mask,
                          const AnimationTimeline& timeline,
                          QString* errorOut) {
    QJsonObject root;
    root["version"]  = QStringLiteral("1.0");
    root["shape"]    = QString::fromStdString(shapeTypeName(mask.shape()));
    root["gridSize"] = mask.gridSize();
    root["fps"]      = timeline.fps();

    const double dur = 1.0 / std::max(1, timeline.fps());
    QJsonArray frames;
    for (int i = 0; i < timeline.frameCount(); ++i) {
        frames.append(frameToJson(timeline.frameAt(i), dur));
    }
    root["frames"] = frames;

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = out.errorString();
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        if (errorOut) *errorOut = out.errorString();
        return false;
    }
    return true;
}

// ── Load ─────────────────────────────────────────────────────────────────────

LoadResult JsonSerializer::load(const QString& path,
                                std::shared_ptr<ShapeMask>* maskOut,
                                AnimationTimeline* timelineOut) {
    LoadResult r;
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        r.errorMessage = in.errorString();
        return r;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        r.errorMessage = QStringLiteral("JSON parse: %1").arg(pe.errorString());
        return r;
    }
    const QJsonObject root = doc.object();

    // Shape
    try {
        r.shape = shapeTypeFromName(root.value("shape").toString("cube").toStdString());
    } catch (const std::exception& e) {
        r.errorMessage = QStringLiteral("unknown shape: %1").arg(e.what());
        return r;
    }

    // Grid size — clamped to [3, 32] (AV-006)
    const int rawGrid = root.value("gridSize").toInt(8);
    r.gridSize        = std::clamp(rawGrid, 3, 32);
    r.gridSizeClamped = (rawGrid != r.gridSize);

    // FPS
    r.fps = std::clamp(root.value("fps").toInt(12), 1, 60);

    // Frames
    const QJsonArray frames = root.value("frames").toArray();
    std::vector<VoxelFrame> parsed;
    parsed.reserve(static_cast<size_t>(frames.size()));
    for (int i = 0; i < frames.size(); ++i) {
        VoxelFrame f;
        if (!readFrame(frames[i].toObject(), f, &r.errorMessage)) {
            r.errorMessage = QStringLiteral("frame %1: %2").arg(i + 1).arg(r.errorMessage);
            return r;
        }
        parsed.push_back(std::move(f));
    }
    if (parsed.empty()) parsed.emplace_back();   // invariant: ≥1 frame

    // Apply
    if (maskOut)     *maskOut = std::make_shared<ShapeMask>(r.gridSize, r.shape);
    if (timelineOut) {
        timelineOut->setFps(r.fps);
        timelineOut->replaceFrames(std::move(parsed), 0);
    }

    r.ok = true;
    return r;
}
