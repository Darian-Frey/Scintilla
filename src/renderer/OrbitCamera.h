#pragma once

#include <QMatrix4x4>
#include <QVector3D>

// ── OrbitCamera ───────────────────────────────────────────────────────────────
//
// Manual spherical-coordinate camera. No OrbitControls-equivalent dependency
// (DEC-007). All state is (theta, phi, radius, target). Defaults match the
// prototype reference (DEC-002 / DEC-007):
//
//   theta  = π/4       (azimuth, unbounded)
//   phi    = π/3.1     (polar, clamped to [0.05, π−0.05])
//   radius = 8 × 1.95  (default; override with fitToGrid())
//   target = (0,0,0)

class OrbitCamera {
public:
    OrbitCamera();

    // ── State setters ─────────────────────────────────────────────────────────
    void setTheta(float theta)               { m_theta  = theta; }
    void setPhi(float phi);                                       // clamped
    void setRadius(float r);                                      // clamped
    void setTarget(const QVector3D& t)       { m_target = t; }

    // ── State getters ─────────────────────────────────────────────────────────
    [[nodiscard]] float       theta()  const { return m_theta; }
    [[nodiscard]] float       phi()    const { return m_phi; }
    [[nodiscard]] float       radius() const { return m_radius; }
    [[nodiscard]] QVector3D   target() const { return m_target; }

    // ── Convenience ───────────────────────────────────────────────────────────
    // Set radius to the default distance for a grid of this size.
    void fitToGrid(int gridSize);

    // Restore default angles (theta=π/4, phi=π/3.1). Preserves radius and target.
    void resetAngles();

    // ── Interaction ───────────────────────────────────────────────────────────
    void orbit(int dx, int dy);   // mouse drag → theta/phi delta
    void pan(int dx, int dy);     // mouse drag (shift / middle) → target shift
    void zoom(int ticks);         // wheel ticks → radius multiplier

    // ── Matrices ──────────────────────────────────────────────────────────────
    [[nodiscard]] QVector3D  position()             const;     // world-space eye
    [[nodiscard]] QMatrix4x4 viewMatrix()           const;
    [[nodiscard]] QMatrix4x4 projMatrix(float aspect) const;

private:
    float     m_theta;
    float     m_phi;
    float     m_radius;
    QVector3D m_target;

    // Tuning constants matched to the prototype.
    static constexpr float kOrbitSensitivity = 0.005f;
    static constexpr float kPanSensitivity   = 0.01f;
    static constexpr float kZoomFactor       = 1.1f;
    static constexpr float kFovDegrees       = 45.0f;
    static constexpr float kNearPlane        = 0.1f;
    static constexpr float kFarPlane         = 500.0f;
    static constexpr float kPhiMin           = 0.05f;
    static constexpr float kPhiMax           = 3.09f;        // π − 0.05
    static constexpr float kRadiusMin        = 2.0f;
    static constexpr float kRadiusMax        = 200.0f;
    static constexpr float kRadiusGridFactor = 1.95f;
};
