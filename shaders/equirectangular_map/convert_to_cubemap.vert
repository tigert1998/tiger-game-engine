#version 460 core
layout (location = 0) in vec3 aPosition;

out vec3 vLocalPosition;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;

void main() {
    vLocalPosition = aPosition;
    gl_Position =  uProjectionMatrix * uViewMatrix * vec4(vLocalPosition, 1.0);
}