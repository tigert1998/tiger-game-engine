#version 460 core

#extension GL_ARB_bindless_texture : require

#include "material.glsl"

layout (std430, binding = 6) buffer materialsBuffer {
    Material materials[]; // per instance
};
layout (std430, binding = 7) buffer texturesBuffer {
    sampler2D textures[];
};

in vec2 gTexCoord;
flat in int gInstanceID;

void main() {
    Material material = materials[gInstanceID];
    float alpha = 1;
    if (material.diffuseTexture >= 0) {
        vec4 sampled = texture(textures[material.diffuseTexture], gTexCoord);
        alpha = sampled.a;
    }
    if (alpha < 0.5) discard;
}