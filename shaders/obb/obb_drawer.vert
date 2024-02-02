#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in mat3 aRotation;

out vec3 vColor;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

void main() {
    vColor = aColor;
    gl_Position = uProjectionMatrix * uViewMatrix * mat4(aRotation) * vec4(aPosition, 1);
}
