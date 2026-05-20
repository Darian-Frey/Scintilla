#include "OrbitCamera.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi      = 3.14159265358979323846f;
    constexpr float kDefaultTheta = kPi / 4.0f;
    constexpr float kDefaultPhi   = kPi / 3.1f;
}

OrbitCamera::OrbitCamera()
    : m_theta(kDefaultTheta)
    , m_phi(kDefaultPhi)
    , m_radius(8.0f * kRadiusGridFactor)
    , m_target(0.0f, 0.0f, 0.0f)
{}

// ── Clamped setters ──────────────────────────────────────────────────────────

void OrbitCamera::setPhi(float phi) {
    m_phi = std::clamp(phi, kPhiMin, kPhiMax);
}

void OrbitCamera::setRadius(float r) {
    m_radius = std::clamp(r, kRadiusMin, kRadiusMax);
}

// ── Convenience ──────────────────────────────────────────────────────────────

void OrbitCamera::fitToGrid(int gridSize) {
    setRadius(static_cast<float>(gridSize) * kRadiusGridFactor);
}

void OrbitCamera::resetAngles() {
    m_theta = kDefaultTheta;
    m_phi   = kDefaultPhi;
}

// ── Interaction ──────────────────────────────────────────────────────────────

void OrbitCamera::orbit(int dx, int dy) {
    m_theta -= static_cast<float>(dx) * kOrbitSensitivity;
    setPhi(m_phi - static_cast<float>(dy) * kOrbitSensitivity);
}

void OrbitCamera::pan(int dx, int dy) {
    // Pan in the camera's local right / up plane, scaled by radius so the pan
    // feels consistent regardless of zoom level.
    const float sinPhi   = std::sin(m_phi);
    const float cosPhi   = std::cos(m_phi);
    const float sinTheta = std::sin(m_theta);
    const float cosTheta = std::cos(m_theta);

    // Forward (from target → eye); we need right = forward × world-up.
    QVector3D right(cosTheta, 0.0f, -sinTheta);
    QVector3D up(-sinTheta * cosPhi, sinPhi, -cosTheta * cosPhi);

    const float s = kPanSensitivity * m_radius;
    m_target -= right * (static_cast<float>(dx) * s);
    m_target += up    * (static_cast<float>(dy) * s);
}

void OrbitCamera::zoom(int ticks) {
    if (ticks == 0) return;
    const float factor = (ticks > 0) ? 1.0f / kZoomFactor : kZoomFactor;
    float r = m_radius;
    for (int i = 0; i < std::abs(ticks); ++i) r *= factor;
    setRadius(r);
}

// ── Matrices ─────────────────────────────────────────────────────────────────

QVector3D OrbitCamera::position() const {
    const float sinPhi = std::sin(m_phi);
    return m_target + QVector3D(
        m_radius * sinPhi * std::sin(m_theta),
        m_radius * std::cos(m_phi),
        m_radius * sinPhi * std::cos(m_theta)
    );
}

QMatrix4x4 OrbitCamera::viewMatrix() const {
    QMatrix4x4 m;
    m.lookAt(position(), m_target, QVector3D(0.0f, 1.0f, 0.0f));
    return m;
}

QMatrix4x4 OrbitCamera::projMatrix(float aspect) const {
    QMatrix4x4 m;
    m.perspective(kFovDegrees, aspect, kNearPlane, kFarPlane);
    return m;
}
