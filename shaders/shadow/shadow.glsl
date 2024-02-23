#ifndef SHADOW_SHADOW_GLSL_
#define SHADOW_SHADOW_GLSL_

struct DirectionalShadow {
    // Cascaded Shadow Mapping
    mat4 viewProjectionMatrices[NUM_CASCADES];
    float cascadePlaneDistances[NUM_CASCADES * 2];
    sampler2DArray shadowMap;
    vec3 dir;
};

struct OmnidirectionalShadow {
    mat4 viewProjectionMatrices[6];
    samplerCube shadowMap;
    vec3 pos;
    float farPlane;
};

float CalcDirectionalShadowForSingleCascade(DirectionalShadow directionalShadow, uint layer, vec3 position) {
    vec4 homoPosition = directionalShadow.viewProjectionMatrices[layer] * vec4(position, 1.0);

    position = homoPosition.xyz / homoPosition.w;
    position = position * 0.5 + 0.5;
    float currentDepth = position.z;
    currentDepth = clamp(currentDepth, 0, 1);
    // If the fragment is outside the shadow frustum, we don't care about its depth.
    // Otherwise, the fragment's depth must be between [0, 1].

    float shadow = 0;
    vec2 texelSize = 1.0 / vec2(textureSize(directionalShadow.shadowMap, 0));
    int numSamples = 0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float closestDepth = texture(
                directionalShadow.shadowMap,
                vec3(position.xy + vec2(x, y) * texelSize, layer)
            ).r;
            shadow += currentDepth > closestDepth ? 1.0 : 0.0;
            numSamples += 1;
        }
    }
    shadow /= numSamples;

    return shadow;
}

float CalcDirectionalShadow(DirectionalShadow directionalShadow, vec3 position, mat4 cameraViewMatrix) {
    // select cascade layer
    vec4 viewSpacePosition = cameraViewMatrix * vec4(position, 1);
    float depth = -viewSpacePosition.z;

    float num = 0;
    float denom = 0;
    for (int i = 0; i < NUM_CASCADES; i++) {
        float near = directionalShadow.cascadePlaneDistances[i * 2];
        float far = directionalShadow.cascadePlaneDistances[i * 2 + 1]; 
        if (near <= depth && depth < far) {
            float shadow = CalcDirectionalShadowForSingleCascade(directionalShadow, i, position);
            float ratio = (far - depth) / (far - near);
            num += shadow * ratio;
            denom += ratio;
        }
    }

    return num / denom;
}

float CalcOmnidirectionalShadow(OmnidirectionalShadow omnidirectionalShadow, vec3 position) {
    vec3 dir = position - omnidirectionalShadow.pos;
    float currentDepth = distance(position, omnidirectionalShadow.pos) / omnidirectionalShadow.farPlane;
    float closestDepth = texture(omnidirectionalShadow.shadowMap, dir).r;
    float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

    return shadow;
}

#endif
