#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_NV_geometry_shader_passthrough : require
#extension GL_NV_viewport_array2 : require

layout (triangles) in;
layout (viewport_relative) out int gl_Layer;

#include "light_sources.glsl"

layout (passthrough) in gl_PerVertex {
    vec4 gl_Position;
} gl_in[];

layout (passthrough) in vOutputs {
    vec3 position;
    vec2 texCoord;
    mat3 TBN;
    flat int instanceID;
} vOut[];

uniform uint uLightIndex;

void main() {
    DirectionalShadow directionalShadow = directionalLights[uLightIndex].shadow;

    int mask = 0;
    for (uint i = 0; i < NUM_CASCADES; i++)
        if (directionalShadow.requiresUpdate[i]) {
            mask |= (1 << i);
        }
    gl_ViewportMask[0] = mask;

    gl_Layer = 0;
}
