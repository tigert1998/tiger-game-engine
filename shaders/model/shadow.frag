#version 460 core

#extension GL_ARB_bindless_texture : require

#include "material.glsl"
#include "shadow/directional_shadow.glsl"
#include "common/alpha_test.glsl"

layout (std430, binding = 6) buffer materialsBuffer {
    Material materials[]; // per instance
};
layout (std430, binding = 7) buffer texturesBuffer {
    sampler2D textures[];
};

in vec2 gTexCoord;
in mat3 gTBN;
flat in int gInstanceID;

void main() {
    Material material = materials[gInstanceID];
    float alpha = 1;
    if (material.diffuseTexture >= 0) {
        vec4 sampled = texture(textures[material.diffuseTexture], gTexCoord);
        alpha = sampled.a;
    }
    AlphaTest(alpha);

    // add bias in the shadow mapping generation stage:
    // According to Arccch's comment in https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
    DirectionalShadow directionalShadow = directionalShadows[0];
    float bias = max(0.05 * (1.0 - dot(gTBN[2], normalize(-directionalShadow.dir))), 0.005);
    const float biasModifier = 0.5;
    if (gl_Layer == NUM_CASCADES - 1) {
        bias *= 1 / (directionalShadow.farPlaneDistance * biasModifier);
    } else {
        bias *= 1 / (directionalShadow.cascadePlaneDistances[gl_Layer] * biasModifier);
    }
    gl_FragDepth = gl_FragCoord.z;
    gl_FragDepth += gl_FrontFacing ? bias : 0; 
}
