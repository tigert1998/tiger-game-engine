#version 420 core

const float zero = 1e-6;

uniform vec3 uCameraPosition;

in vec3 vPosition;
in vec2 vTexCoord;
in vec3 vNormal;

#include "light_sources.glsl"

out vec4 fragColor;

vec4 CalcFragColor() {
    vec3 lightGreen = vec3(139.0, 205.0, 80.0) / 255.0;
    vec3 darkGreen = vec3(53.0, 116.0, 32.0) / 255.0;
    vec3 green = mix(darkGreen, lightGreen, vTexCoord.y);

    float facing = dot(vPosition - uCameraPosition, -vNormal);
    vec3 normal = facing > 0 ? vNormal : -vNormal;

    vec3 color = CalcPhongLighting(
        green, green, vec3(zero),
        normal, uCameraPosition, vPosition,
        0, 0
    );

    return vec4(color, 1.0);
}

void main() {
    fragColor = CalcFragColor();
}
