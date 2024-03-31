#version 460 core

#extension GL_ARB_bindless_texture : require

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

#include "light_sources.glsl"

in vOutputs {
    vec3 position;
    vec2 texCoord;
    mat3 TBN;
    flat int instanceID;
} vOut[];

out vec2 gTexCoord;
out mat3 gTBN;
flat out int gInstanceID;
out vec3 gPosition;

uniform uint uLightIndex;

void main() {
    OmnidirectionalShadow omnidirectionalShadow = pointLights[uLightIndex].shadow;

    for (int face = 0; face < 6; face++) {
        gl_Layer = face;
        for (int i = 0; i < 3; i++) {
            gPosition = vOut[i].position;
            gl_Position = omnidirectionalShadow.viewProjectionMatrices[face] * vec4(vOut[i].position, 1);
            gTexCoord = vOut[i].texCoord;
            gTBN = vOut[i].TBN;
            gInstanceID = vOut[i].instanceID;
            EmitVertex();
        }
        EndPrimitive();
    }
}
