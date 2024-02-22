#version 460 core

#extension GL_ARB_bindless_texture : require

#include "material.glsl"
#include "light_sources.glsl"
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
in vec3 gPosition;

uniform uint uLightIndex;

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
    OmnidirectionalShadow omnidirectionalShadow = pointLights[uLightIndex].shadow;
    vec3 dir = normalize(omnidirectionalShadow.pos - gPosition);
    float bias = max(0.05 * (1.0 - dot(gTBN[2], dir)), 0.005);
    gl_FragDepth = distance(omnidirectionalShadow.pos, gPosition) / omnidirectionalShadow.farPlane;
    gl_FragDepth += gl_FrontFacing ? bias : 0; 
}
