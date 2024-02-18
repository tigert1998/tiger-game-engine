#ifndef SHADOW_DIRECTIONAL_SHADOW_GLSL_
#define SHADOW_DIRECTIONAL_SHADOW_GLSL_

struct DirectionalShadow {
    // Cascaded Shadow Mapping
    mat4 viewProjectionMatrices[NUM_CASCADES];
    float cascadePlaneDistances[NUM_CASCADES - 1];
    float farPlaneDistance;
    sampler2DArray shadowMap;
    vec3 dir;
};

uniform uint uNumDirectionalShadows;

layout (std430, binding = DIRECTIONAL_SHADOW_BINDING) buffer directionalShadowsBuffer {
    DirectionalShadow directionalShadows[];
};

#endif
