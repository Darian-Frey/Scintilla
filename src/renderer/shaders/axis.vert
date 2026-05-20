#version 430 core

// Per-vertex position + colour. Used for both line segments (axes) and
// points (axis tips) of the corner orientation gizmo (F-042).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uVP;

out vec3 vColor;

void main() {
    gl_Position  = uVP * vec4(aPos, 1.0);
    gl_PointSize = 8.0;   // applies only when drawing GL_POINTS
    vColor       = aColor;
}
