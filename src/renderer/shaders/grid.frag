#version 430 core

out vec4 fragColor;

// Bounding-box appearance — matches prototype (SPEC §3.1):
//   - colour: warm white
//   - opacity: 0.11
const vec3  kGridColor   = vec3(0.85, 0.85, 0.90);
const float kGridOpacity = 0.11;

void main() {
    fragColor = vec4(kGridColor, kGridOpacity);
}
