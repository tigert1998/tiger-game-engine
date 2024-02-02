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

    const float kShadowBiasFactor = 1e-3;

    position = homoPosition.xyz / homoPosition.w;
    position = position * 0.5 + 0.5;
    float currentDepth = position.z;
    currentDepth = max(min(currentDepth, 1), 0);
    // If the fragment is outside the shadow frustum, we don't care about its depth.
    // Otherwise, the fragment's depth must be between [0, 1].

    float bias = max(0.05 * (1.0 - dot(normal, -uDirectionalShadow.dir)), 0.005);
    const float biasModifier = 0.5f;
    if (layer == NUM_CASCADES - 1) {
        bias *= 1 / (uDirectionalShadow.farPlaneDistance * biasModifier);
    } else {
        bias *= 1 / (uDirectionalShadow.cascadePlaneDistances[layer] * biasModifier);
    }

    float shadow = 0;
    vec2 texelSize = 1.0 / vec2(textureSize(uDirectionalShadow.shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(
                uDirectionalShadow.shadowMap, 
                vec3(position.xy + vec2(x, y) * texelSize, layer)
            ).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
