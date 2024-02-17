#ifndef COMMON_POSITION_FROM_DEPTH_GLSL_
#define COMMON_POSITION_FROM_DEPTH_GLSL_

vec3 PositionFromDepth(mat4 viewProjectionMatrix, vec2 uv, float depth) {
    float far = gl_DepthRange.far;
    float near = gl_DepthRange.near;
    vec4 clipSpacePosition = vec4(uv * 2 - 1, (2 * depth - near - far) / (far - near), 1);
    vec4 position = inverse(viewProjectionMatrix) * clipSpacePosition;
    position /= position.w;
    return position.xyz;
}

#endif
