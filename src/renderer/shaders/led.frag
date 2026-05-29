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

    // The LED's own brightness — used so dim colours look dim rather than
    // washing out to white at the centre, and so unlit (0,0,0) cells
    // genuinely emit nothing.
    float ledBrightness = max(max(vColor.r, vColor.g), vColor.b);

    // Centre washes toward white at high brightness; at low brightness
    // both the white target and the mix factor scale down so the colour
    // identity survives.
    vec3 hot = mix(vColor,
                   vec3(1.8) * ledBrightness,
                   core * 0.9 * ledBrightness);

    // Intensity envelope: bright at centre, soft at the rim. Scaled by
    // brightness so a 25 %-red LED genuinely looks ~25 % as bright as full
    // red rather than the same with a tinted rim.
    float intensity = (dome * 0.85 + core * 1.6) * ledBrightness;

    // Emissive base — already scaled by vColor, so dim colours naturally
    // contribute less.
    vec3 emissive = vColor * 0.45;

    vec3 finalColor = emissive + hot * intensity;

    // alpha = 1.0 because we render with additive blending (GL_ONE/GL_ONE);
    // the colour itself encodes the falloff. Edges have low intensity so
    // they contribute almost nothing additively.
    fragColor = vec4(finalColor, 1.0);
}
