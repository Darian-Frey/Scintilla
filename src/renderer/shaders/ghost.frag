#version 430 core

in vec3  vNormal;
in float vScale;

out vec4 fragColor;

// Ghost LED appearance — matches prototype (DEC-002):
//   - colour: warm white
//   - opacity: 0.22
const vec3  kGhostColor   = vec3(0.85, 0.85, 0.90);
const float kGhostOpacity = 0.22;

void main() {
    if (vScale < 0.001) discard;

    // Soft directional shading so the ghosts read as spheres rather than flat dots
    vec3  N    = normalize(vNormal);
    float lit  = max(dot(N, normalize(vec3(0.3, 0.6, 0.7))), 0.0) * 0.55 + 0.45;

    fragColor = vec4(kGhostColor * lit, kGhostOpacity);
}
