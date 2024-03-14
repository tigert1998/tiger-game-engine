#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 uViewProjectionMatrixX;
uniform mat4 uViewProjectionMatrixY;
uniform mat4 uViewProjectionMatrixZ;

in vec2 vTexCoord[3];
in mat3 vTBN[3];
flat in int vInstanceID[3];
out vec2 gTexCoord;
out mat3 gTBN;
flat out int gInstanceID;
flat out int gAxis;

void main() {
    vec3 edge1 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 edge2 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
    vec3 normalAbs = abs(normalize(cross(edge1, edge2)));

    mat4 viewProjectionMatrix;
    if (normalAbs.x >= normalAbs.y && normalAbs.x >= normalAbs.z) {
        viewProjectionMatrix = uViewProjectionMatrixX;
        gAxis = 0;
    } else if (normalAbs.y >= normalAbs.x && normalAbs.y >= normalAbs.z) {
        viewProjectionMatrix = uViewProjectionMatrixY;
        gAxis = 1;
    } else {
        viewProjectionMatrix = uViewProjectionMatrixZ;
        gAxis = 2;
    }

    for (int i = 0; i < 3; i++) {
        gl_Position = viewProjectionMatrix * gl_in[i].gl_Position;
        gTexCoord = vTexCoord[i];
        gTBN = vTBN[i];
        gInstanceID = vInstanceID[i];
        EmitVertex();
    }
    EndPrimitive();
}
