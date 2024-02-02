#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "smaa/common_defines.glsl"
#include "smaa.hlsl"

in vec4 vOffset[3];
in vec2 vTexCoord;
in vec2 vPixCoord;

uniform sampler2D uEdges;
uniform sampler2D uArea;
uniform sampler2D uSearch;

out vec4 fragColor;

void main() {
    fragColor = SMAABlendingWeightCalculationPS(vTexCoord, vPixCoord, vOffset, uEdges, uArea, uSearch, vec4(0));
    // gamma correction
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / 2.2));
}