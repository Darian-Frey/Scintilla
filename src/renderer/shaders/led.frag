#version 430 core

in vec3  vColor;
in vec3  vNormal;
in vec3  vWorldPos;
in float vScale;

uniform vec3 uLightDir;
uniform vec3 uCamPos;

out vec4 fragColor;

// Real-LED look without screen-space bloom:
//
//   - Fresnel-driven brightness so the visible "lit area" is a small core
//     inside the geometric sphere; the rim fades to a soft glow rather than
//     a hard silhouette. This makes the lit LED appear visually the same
//     size as the ghost dot underneath it (DEC-002 sphere radius preserved).
//
//   - Mix toward white at the very front-facing centre, mimicking how a
//     real LED die looks through a diffuser dome — the colour washes out
//     where the light is hottest.
//
//   - Strong emissive contribution so lit LEDs self-illuminate; they don't
//     depend on the directional light to be visible.
//
// Paired with additive blending in CubeViewport::paintGL so overlapping
// LEDs accumulate brightness (physically what happens when multiple light
// sources are seen together).

void main() {
    if (vScale < 0.001) discard;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);

    float NdotV = max(dot(N, V), 0.0);

    // Very tight hot spot at the front-facing centre (white-ish die)
    float core = pow(NdotV, 8.0);

    // Wider but still front-biased dome glow
    float dome = pow(NdotV, 2.0);

    // Centre washes toward white; edges keep the LED's colour
    vec3 hot = mix(vColor, vec3(1.8), core * 0.9);

    // Intensity envelope: bright at centre, soft at the rim
    float intensity = dome * 0.85 + core * 1.6;

    // Emissive base so the LED looks self-lit even when behind shadows
    vec3 emissive = vColor * 0.45;

    vec3 finalColor = emissive + hot * intensity;

    // alpha = 1.0 because we render with additive blending (GL_ONE/GL_ONE);
    // the colour itself encodes the falloff. Edges have low intensity so
    // they contribute almost nothing additively.
    fragColor = vec4(finalColor, 1.0);
}
