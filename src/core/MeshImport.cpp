#include "MeshImport.h"

#include <QDataStream>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

namespace MeshImport {

namespace {

QVector3D readVec3(QDataStream& ds) {
    float x = 0, y = 0, z = 0;
    ds >> x >> y >> z;
    return {x, y, z};
}

std::vector<Triangle> loadStlBinary(QFile& f, QString* error) {
    f.seek(0);
    if (f.read(80).size() != 80) {
        if (error) *error = QStringLiteral("Binary STL too short for 80-byte header.");
        return {};
    }
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 count = 0;
    ds >> count;

    std::vector<Triangle> tris;
    tris.reserve(count);

    for (quint32 i = 0; i < count; ++i) {
        readVec3(ds);                      // facet normal — ignored
        Triangle t;
        t.v0 = readVec3(ds);
        t.v1 = readVec3(ds);
        t.v2 = readVec3(ds);
        quint16 attr = 0;
        ds >> attr;                        // attribute byte count — ignored

        if (ds.status() != QDataStream::Ok) {
            if (error) {
                *error = QString("Binary STL truncated at triangle %1 of %2.")
                             .arg(i).arg(count);
            }
            return tris;
        }
        tris.push_back(t);
    }
    return tris;
}

std::vector<Triangle> loadStlAscii(QFile& f, QString* error) {
    f.seek(0);
    QTextStream ts(&f);
    std::vector<Triangle> tris;

    static const QRegularExpression kSplitter(QStringLiteral("\\s+"));

    Triangle current;
    int vertexCount = 0;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (!line.startsWith(QStringLiteral("vertex"), Qt::CaseInsensitive)) continue;
        const QStringList parts = line.split(kSplitter, Qt::SkipEmptyParts);
        if (parts.size() < 4) continue;
        const QVector3D v(parts[1].toFloat(),
                          parts[2].toFloat(),
                          parts[3].toFloat());
        if (vertexCount == 0)      current.v0 = v;
        else if (vertexCount == 1) current.v1 = v;
        else if (vertexCount == 2) current.v2 = v;
        if (++vertexCount == 3) {
            tris.push_back(current);
            vertexCount = 0;
        }
    }
    if (tris.empty() && error) {
        *error = QStringLiteral("No triangles found in ASCII STL.");
    }
    return tris;
}

}  // namespace

std::vector<Triangle> loadStl(const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        if (error) *error = QString("Cannot open %1: %2").arg(path, f.errorString());
        return {};
    }

    // Binary STL test: file size should match 84 + 50 * triangleCount,
    // where triangleCount is the uint32 at offset 80. ASCII STLs that
    // happen to start with "solid" will fail this test and fall through
    // to the ASCII parser.
    bool binary = false;
    const qint64 sz = f.size();
    if (sz >= 84) {
        f.seek(80);
        QDataStream ds(&f);
        ds.setByteOrder(QDataStream::LittleEndian);
        quint32 count = 0;
        ds >> count;
        if (qint64(84) + qint64(count) * 50 == sz) binary = true;
    }

    if (binary) return loadStlBinary(f, error);

    f.seek(0);
    const QByteArray head = f.read(5);
    if (head.startsWith("solid")) return loadStlAscii(f, error);

    if (error) {
        *error = QStringLiteral("Unrecognised STL format — neither binary "
                                "nor ASCII signature matched.");
    }
    return {};
}

std::vector<VoxelKey> voxelise(const std::vector<Triangle>& tris,
                               int gridSize,
                               int minSamples,
                               int maxSamples) {
    if (tris.empty() || gridSize < 1) return {};

    // ── 1. Bounding box of all vertices ─────────────────────────────────
    QVector3D mn = tris.front().v0;
    QVector3D mx = mn;
    auto extend = [&](const QVector3D& v) {
        mn.setX(std::min(mn.x(), v.x()));
        mn.setY(std::min(mn.y(), v.y()));
        mn.setZ(std::min(mn.z(), v.z()));
        mx.setX(std::max(mx.x(), v.x()));
        mx.setY(std::max(mx.y(), v.y()));
        mx.setZ(std::max(mx.z(), v.z()));
    };
    for (const auto& t : tris) { extend(t.v0); extend(t.v1); extend(t.v2); }

    // ── 2. Auto-fit into the cube with a 10 % margin ────────────────────
    const QVector3D span    = mx - mn;
    const float maxSpan     = std::max({span.x(), span.y(), span.z(), 1e-6f});
    const float scale       = (gridSize - 1) * 0.9f / maxSpan;
    const QVector3D meshC   = (mn + mx) * 0.5f;
    const QVector3D cubeC   = QVector3D(gridSize - 1, gridSize - 1, gridSize - 1) * 0.5f;
    const auto toCube = [&](const QVector3D& v) -> QVector3D {
        return (v - meshC) * scale + cubeC;
    };

    // ── 3. Sample each triangle in cube space ───────────────────────────
    std::set<VoxelKey> voxels;
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

    for (const auto& t : tris) {
        const QVector3D a = toCube(t.v0);
        const QVector3D b = toCube(t.v1);
        const QVector3D c = toCube(t.v2);

        // Samples scale with triangle area so large faces get enough
        // coverage to hit every voxel they touch.
        const float area2 = QVector3D::crossProduct(b - a, c - a).length();
        int samples = static_cast<int>(area2 * 1.5f);
        samples = std::clamp(samples, minSamples, maxSamples);

        for (int i = 0; i < samples; ++i) {
            float u = uniform(rng);
            float v = uniform(rng);
            if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
            const QVector3D p = a * (1.0f - u - v) + b * u + c * v;
            const int ix = std::clamp(int(std::round(p.x())), 0, gridSize - 1);
            const int iy = std::clamp(int(std::round(p.y())), 0, gridSize - 1);
            const int iz = std::clamp(int(std::round(p.z())), 0, gridSize - 1);
            voxels.insert({ix, iy, iz});
        }
    }

    return {voxels.begin(), voxels.end()};
}

}  // namespace MeshImport
