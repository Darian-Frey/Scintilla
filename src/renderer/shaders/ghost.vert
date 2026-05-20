#version 430 core

// ── Per-vertex (unit sphere geometry) ────────────────────────────────────────
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

// ── Per-instance (LedInstance struct, stride = 32 bytes) ─────────────────────
layout(location = 2) in vec3  iWorldPos;   // offset 0
layout(location = 3) in vec3  iColor;      // offset 12 (unused for ghosts)
layout(location = 4) in float iScale;      // offset 24

// ── Uniforms ─────────────────────────────────────────────────────────────────
uniform mat4 uVP;

// ── Outputs ──────────────────────────────────────────────────────────────────
out vec3  vNormal;
out vec3  vColor;     // non-zero for slice-hidden lit cells (the "glow" state)
out float vScale;

// Ghost LED radius — matches prototype constant 0.17 (DEC-002).
const float kGhostRadius = 0.17;

void main() {
    vec3 scaledPos = iWorldPos + aPos * kGhostRadius * iScale;
    gl_Position    = uVP * vec4(scaledPos, 1.0);

    vNormal = aNormal;
    vColor  = iColor;
    vScale  = iScale;
}
