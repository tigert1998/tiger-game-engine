#version 410 core

layout (location = 0) in vec3 aPosition;

out vec3 vTexCoord;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;

void main() {
    vTexCoord = aPosition;
    gl_Position = uProjectionMatrix * uViewMatrix * vec4(aPosition, 1);
}
