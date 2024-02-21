#ifndef SHADOW_SHADOW_BUFFERS_GLSL_
#define SHADOW_SHADOW_BUFFERS_GLSL_

struct DirectionalShadow {
    // Cascaded Shadow Mapping
    mat4 viewProjectionMatrices[NUM_CASCADES];
    float cascadePlaneDistances[NUM_CASCADES * 2];
    sampler2DArray shadowMap;
    vec3 dir;
};

uniform uint uNumDirectionalShadows;

layout (std430, binding = DIRECTIONAL_SHADOW_BINDING) buffer directionalShadowsBuffer {
    DirectionalShadow directionalShadows[];
};

struct OmnidirectionalShadow {
    mat4 viewProjectionMatrices[6];
    samplerCube shadowMap;
    vec3 pos;
};

uniform uint uNumOmnidirectionalShadows;

layout (std430, binding = OMNIDIRECTIONAL_SHADOW_BINDING) buffer omnidirectionalShadowsBuffer {
    OmnidirectionalShadow omnidirectionalShadows[];
};

#endif
