#version 460 core

#extension GL_ARB_bindless_texture : require

layout (triangles, invocations = NUM_CASCADES) in;
layout (triangle_strip, max_vertices = 3) out;

#include "light_sources.glsl"

in vec2 vTexCoord[3];
in mat3 vTBN[3];
flat in int vInstanceID[3];
out vec2 gTexCoord;
out mat3 gTBN;
flat out int gInstanceID;

uniform uint uLightIndex;
uniform bool uEnableGlobalCascade;

void main() {
    if (!uEnableGlobalCascade && gl_InvocationID == NUM_CASCADES - 1) return;

    DirectionalShadow directionalShadow = directionalLights[uLightIndex].shadow;

    for (int i = 0; i < 3; ++i) {
        gl_Position = directionalShadow.viewProjectionMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        gTexCoord = vTexCoord[i];
        gTBN = vTBN[i];
        gInstanceID = vInstanceID[i];
        EmitVertex();
    }
    EndPrimitive();
}
