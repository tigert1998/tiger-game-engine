struct DirectionalShadow {
    // Cascaded Shadow Mapping
    bool enabled;

    mat4 viewProjectionMatrices[NUM_CASCADES];
    float cascadePlaneDistances[NUM_CASCADES - 1];
    float farPlaneDistance;
    sampler2DArray shadowMap;
    vec3 dir;
};

uniform DirectionalShadow uDirectionalShadow;

float CalcShadow(vec3 position, vec3 normal) {
    if (!uDirectionalShadow.enabled) {
        return 0;
    }

    // select cascade layer
    vec4 viewSpacePosition = uViewMatrix * vec4(position, 1);
    float depth = -viewSpacePosition.z;

    int layer = NUM_CASCADES - 1;
    for (int i = 0; i < NUM_CASCADES - 1; i++) {
        if (depth < uDirectionalShadow.cascadePlaneDistances[i]) {
            layer = i;
            break;
        }
    }

    vec4 homoPosition = uDirectionalShadow.viewProjectionMatrices[layer] * vec4(position, 1.0);

    position = homoPosition.xyz / homoPosition.w;
    position = position * 0.5 + 0.5;
    float currentDepth = position.z;
    currentDepth = max(min(currentDepth, 1), 0);
    // If the fragment is outside the shadow frustum, we don't care about its depth.
    // Otherwise, the fragment's depth must be between [0, 1].

    float bias = max(0.05 * (1.0 - dot(normal, normalize(-uDirectionalShadow.dir))), 0.005);
    const float biasModifier = 0.5f;
    if (layer == NUM_CASCADES - 1) {
        bias *= 1 / (uDirectionalShadow.farPlaneDistance * biasModifier);
    } else {
        bias *= 1 / (uDirectionalShadow.cascadePlaneDistances[layer] * biasModifier);
    }

    float shadow = 0;
    vec2 texelSize = 1.0 / vec2(textureSize(uDirectionalShadow.shadowMap, 0));
    int numSamples = 0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float closestDepth = texture(
                uDirectionalShadow.shadowMap,
                vec3(position.xy + vec2(x, y) * texelSize, layer)
            ).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
            numSamples += 1;
        }
    }
    shadow /= numSamples;

    return shadow;
}
