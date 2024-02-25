#ifndef SHADOW_SHADOW_GLSL_
#define SHADOW_SHADOW_GLSL_

#include "common/rand.glsl"

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
    float radius;
    float farPlane;
};

layout (std430, binding = POISSON_DISK_2D_BINDING) buffer poissonDisk2DPointsBuffer {
    vec2 poissonDisk2DPoints[];
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
    const int numSamples = 25;
    const float offset = 0.001;

    int sampleStart = int(rand(position.xy) * (poissonDisk2DPoints.length() - numSamples));
    for (int i = sampleStart; i < sampleStart + numSamples; i++) {
        vec2 delta = poissonDisk2DPoints[i] * (2 * offset) - offset;
        float closestDepth = texture(
            directionalShadow.shadowMap,
            vec3(position.xy + delta, layer)
        ).r;
        shadow += currentDepth > closestDepth ? 1.0 : 0.0;
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
    // PCSS
    vec3 dir = position - omnidirectionalShadow.pos;
    vec3 normalizedDir = normalize(dir);
    float currentDepth = distance(position, omnidirectionalShadow.pos) / omnidirectionalShadow.farPlane;

    float blockDepth = 0;
    {
        // blocker search
        int count = 0;
        float offset = 0.1;
        const int numSamples = 3;
        for (int x = 0; x < numSamples; x++) {
            for (int y = 0; y < numSamples; y++) {
                for (int z = 0; z < numSamples; z++) {
                    vec3 delta = vec3(x, y, z) / numSamples * (2 * offset) - offset; // [-offset, offset]
                    float depth = texture(omnidirectionalShadow.shadowMap, normalizedDir + delta).r;
                    if (depth < currentDepth) {
                        // is a blocker
                        count++;
                        blockDepth += depth;
                    }
                }
            }
        }
        if (count == 0) return 0;
        blockDepth /= count;
    }

    float shadow = 0;
    {
        // sampling
        float offset = (omnidirectionalShadow.radius / blockDepth) * (currentDepth - blockDepth);
        const int numSamples = 4;
        for (int x = 0; x < numSamples; x++)
            for (int y = 0; y < numSamples; y++)
                for (int z = 0; z < numSamples; z++) {
                    vec3 delta = vec3(x, y, z) / numSamples * (2 * offset) - offset;
                    float depth = texture(omnidirectionalShadow.shadowMap, dir + delta).r;
                    shadow += int(depth < currentDepth);
                }
        shadow /= pow(numSamples, 3);
    }

    return shadow;
}

#endif
