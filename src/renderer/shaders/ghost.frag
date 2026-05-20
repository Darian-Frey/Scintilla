#version 430 core

in vec3  vNormal;
in vec3  vColor;
in float vScale;

out vec4 fragColor;

// Ghost LED appearance — matches prototype (DEC-002):
//   - default colour: warm white at low opacity (vague outline)
//   - "glow" colour: per-LED colour at higher opacity (slice-hidden lit cells)
const vec3  kDefaultGhostColor = vec3(0.85, 0.85, 0.90);
const float kDefaultOpacity    = 0.22;
const float kGlowOpacity       = 0.70;   // lit-but-hidden cells

void main() {
    if (vScale < 0.001) discard;

    // Soft directional shading so the ghosts read as spheres, not flat dots.
    vec3  N    = normalize(vNormal);
    float lit  = max(dot(N, normalize(vec3(0.3, 0.6, 0.7))), 0.0) * 0.55 + 0.45;

    // Non-zero iColor signals "this slice-hidden cell is lit on another layer"
    // (see LedInstanceBuffer::updateFrame). Render it as a coloured glow at
    // higher opacity so the user can see which voxels are painted across the
    // whole cube while editing a single slice.
    float litness = max(max(vColor.r, vColor.g), vColor.b);
    bool  isGlow  = litness > 0.001;

    vec3  baseColor = isGlow ? vColor             : kDefaultGhostColor;
    float opacity   = isGlow ? kGlowOpacity       : kDefaultOpacity;

    // Final opacity also scales with vScale so the dim-hidden / full-active
    // size differentiation still applies.
    fragColor = vec4(baseColor * lit, opacity * vScale);
}
