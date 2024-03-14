#version 460 core

layout (location = 0) in vec3 aPosition;
uniform float uWorldSize;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

out vec3 vPosition;

void main() {
	vPosition = aPosition * 0.5 * uWorldSize;
    gl_Position = uProjectionMatrix * uViewMatrix * vec4(vPosition, 1.0);
}