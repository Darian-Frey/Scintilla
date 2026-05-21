#version 430 core

// ── Per-vertex (sphere geometry) ──────────────────────────────────────────────
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

// ── Per-instance (LedInstance struct, stride = 32 bytes) ─────────────────────
layout(location = 2) in vec3  iWorldPos;   // offset 0
layout(location = 3) in vec3  iColor;      // offset 12
layout(location = 4) in float iScale;      // offset 24

// ── Uniforms ──────────────────────────────────────────────────────────────────
uniform mat4 uVP;
uniform vec3 uLightDir;   // normalised world-space
uniform vec3 uCamPos;

// ── Outputs ───────────────────────────────────────────────────────────────────
out vec3 vColor;
out vec3 vNormal;
out vec3 vWorldPos;
out float vScale;

// LED sphere radius. Default 0.095 (DEC-028) but user-tunable at runtime
// via the "LED size" slider in the status bar. See DEC-002 / DEC-028 for
// the value history.
uniform float uLedRadius;

void main() {
    vec3 scaledPos = iWorldPos + aPos * uLedRadius * iScale;
    gl_Position    = uVP * vec4(scaledPos, 1.0);

    // Pass through for fragment lighting
    vColor    = iColor;
    vNormal   = aNormal;
    vWorldPos = scaledPos;
    vScale    = iScale;
}
