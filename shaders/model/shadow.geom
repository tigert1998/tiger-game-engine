#version 460 core

layout (triangles, invocations = NUM_CASCADES) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 uDirectionalShadowViewProjectionMatrices[NUM_CASCADES];

void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = uDirectionalShadowViewProjectionMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
