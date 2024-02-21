#version 460 core

#extension GL_ARB_bindless_texture : require

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

#include "shadow/shadow_buffers.glsl"

in vec2 vTexCoord[3];
in mat3 vTBN[3];
flat in int vInstanceID[3];
out vec2 gTexCoord;
out mat3 gTBN;
flat out int gInstanceID;
out vec3 gPosition;

uniform uint uShadowIndex;

void main() {
    OmnidirectionalShadow omnidirectionalShadow = omnidirectionalShadows[uShadowIndex];

    for (int face = 0; face < 6; face++) {
        gl_Layer = face;
        for (int i = 0; i < 3; i++) {
            gPosition = vec3(gl_in[i].gl_Position);
            gl_Position = omnidirectionalShadow.viewProjectionMatrices[face] * gl_in[i].gl_Position;
            gTexCoord = vTexCoord[i];
            gTBN = vTBN[i];
            gInstanceID = vInstanceID[i];
            EmitVertex();
        }
        EndPrimitive();
    }
}
