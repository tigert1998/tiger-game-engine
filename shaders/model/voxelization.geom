#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 uViewProjectionMatrixX;
uniform mat4 uViewProjectionMatrixY;
uniform mat4 uViewProjectionMatrixZ;

in vOutputs {
    vec3 position;
    vec2 texCoord;
    mat3 TBN;
    flat int instanceID;
} vOut[];
out vec2 gTexCoord;
out mat3 gTBN;
flat out int gInstanceID;
flat out int gAxis;

void main() {
    vec3 edge1 = vOut[1].position - vOut[0].position;
    vec3 edge2 = vOut[2].position - vOut[0].position;
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
        gl_Position = viewProjectionMatrix * vec4(vOut[i].position, 1);
        gTexCoord = vOut[i].texCoord;
        gTBN = vOut[i].TBN;
        gInstanceID = vOut[i].instanceID;
        EmitVertex();
    }
    EndPrimitive();
}
