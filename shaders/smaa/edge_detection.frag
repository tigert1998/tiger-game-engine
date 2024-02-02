#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "smaa/common_defines.glsl"
#include "smaa.hlsl"

uniform sampler2D uColor;

in vec4 vOffset[3];
in vec2 vTexCoord;

out vec4 fragColor;

void main() {
    fragColor = vec4(SMAALumaEdgeDetectionPS(vTexCoord, vOffset, uColor), vec2(0));
}
