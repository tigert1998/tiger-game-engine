#version 410 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in mat4 aBladeTransform;
layout (location = 7) in vec3 aBladePosition;
layout (location = 8) in vec2 aBladeTexCoord;

out vec3 vPosition;
out vec2 vTexCoord;
out vec3 vNormal;

uniform vec2 uWindFrequency;
uniform float uTime;
uniform sampler2D uDistortionTexture;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

#include "common/rotate.glsl"

mat4 CalcWindRotation() {
    vec2 uv = aBladeTexCoord + uWindFrequency * uTime;
    vec2 sampled = texture(uDistortionTexture, uv).xy * 2.0f - 1.0f;
    return Rotate(
        vec3(sampled.x, 0, sampled.y),
        PI * 0.5 * length(sampled)
    );
}

void main() {
    mat4 modelMatrix = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(aBladePosition, 1.0)
    ) * CalcWindRotation() * aBladeTransform;
    gl_Position = uProjectionMatrix * uViewMatrix * modelMatrix * vec4(aPosition, 1);
    vPosition = vec3(modelMatrix * vec4(aPosition, 1));
    vTexCoord = aTexCoord;
    vNormal = vec3(transpose(inverse(modelMatrix)) * vec4(aNormal, 0));
}