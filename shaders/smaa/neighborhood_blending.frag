#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "smaa/common_defines.glsl"
#include "smaa.hlsl"

in vec4 vOffset;
in vec2 vTexCoord;

uniform sampler2D uColor;
uniform sampler2D uBlend;

out vec4 fragColor;

void main() {
    fragColor = SMAANeighborhoodBlendingPS(vTexCoord, vOffset, uColor, uBlend);
}
