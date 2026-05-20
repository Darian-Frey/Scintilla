#version 430 core

in vec3  vColor;
in vec3  vNormal;
in vec3  vWorldPos;
in float vScale;

uniform vec3 uLightDir;
uniform vec3 uCamPos;

out vec4 fragColor;

void main() {
    // Discard zero-scale instances (scale-to-zero hide strategy, DEC-001)
    if (vScale < 0.001) discard;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Lambertian diffuse + ambient
    float diff = max(dot(N, L), 0.0) * 0.70 + 0.30;

    // Specular highlight — simulates the glass dome of an LED
    float spec = pow(max(dot(N, H), 0.0), 48.0) * 0.55;

    // Subtle emissive boost so lit LEDs glow even in shadow
    vec3 emissive = vColor * 0.12;

    vec3 lit = vColor * diff + vec3(spec) + emissive;

    fragColor = vec4(lit, 1.0);
}
