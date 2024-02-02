#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in ivec4 aBoneIDs0;
layout (location = 5) in ivec4 aBoneIDs1;
layout (location = 6) in ivec4 aBoneIDs2;
layout (location = 7) in vec4 aBoneWeights0;
layout (location = 8) in vec4 aBoneWeights1;
layout (location = 9) in vec4 aBoneWeights2;

out vec3 vPosition;
out vec2 vTexCoord;
out mat3 vTBN;
flat out int vInstanceID;

layout (std430, binding = 0) buffer modelMatricesBuffer {
    mat4 modelMatrices[]; // per instance
};
layout (std430, binding = 1) buffer boneMatricesBuffer {
    mat4 boneMatrices[];
};
layout (std430, binding = 2) buffer boneMatricesOffsetBuffer {
    int boneMatricesOffset[]; // per instance
};
layout (std430, binding = 3) buffer animatedBuffer {
    uint animated[]; // per instance
};
layout (std430, binding = 4) buffer transformsBuffer {
    mat4 transforms[]; // per instance
};
layout (std430, binding = 5) buffer clipPlanesBuffer {
    vec4 clipPlanes[]; // per instance
};

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

mat4 CalcBoneMatrix() {
    int offset = boneMatricesOffset[vInstanceID];
    mat4 boneMatrix = mat4(0);
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs0[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs0[i] + offset] * aBoneWeights0[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs1[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs1[i] + offset] * aBoneWeights1[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs2[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs2[i] + offset] * aBoneWeights2[i];
    }
    return boneMatrix;
}

void main() {
    vInstanceID = gl_BaseInstance + gl_InstanceID;
    mat4 transform;
    if (bool(animated[vInstanceID])) {
        transform = CalcBoneMatrix();
    } else {
        transform = transforms[vInstanceID];
    }
    mat4 modelMatrix = modelMatrices[vInstanceID];
    if (IS_SHADOW_PASS) {
        gl_Position = modelMatrix * transform * vec4(aPosition, 1);
    } else {
        gl_Position = uProjectionMatrix * uViewMatrix * modelMatrix * transform * vec4(aPosition, 1);
        vPosition = vec3(modelMatrix * transform * vec4(aPosition, 1));
        vTexCoord = aTexCoord;

        mat3 normalMatrix = transpose(inverse(mat3(modelMatrix * transform)));
        vec3 T = normalize(normalMatrix * aTangent);
        vec3 N = normalize(normalMatrix * aNormal);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        vTBN = mat3(T, B, N);

        gl_ClipDistance[0] = dot(vec4(vPosition, 1), clipPlanes[vInstanceID]);
    }
}
