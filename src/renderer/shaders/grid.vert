#version 430 core

// Bounding-box wireframe — non-instanced, simple line geometry.
layout(location = 0) in vec3 aPos;

uniform mat4 uVP;

void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
}
