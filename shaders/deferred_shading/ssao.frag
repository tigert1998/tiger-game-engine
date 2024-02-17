#version 430 core

uniform sampler2D normal;
uniform sampler2D depth;
uniform sampler2D uNoiseTexture;
uniform vec2 uScreenSize;
uniform float uRadius;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

#include "common/position_from_depth.glsl"

layout (std430, binding = 0) buffer kernelsBuffer {
    vec4 kernels[];
};

out float fragColor;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec3 position = PositionFromDepth(
        uProjectionMatrix * uViewMatrix,
        coord, texture(depth, coord).r
    );
    vec3 normalVector = texture(normal, coord).xyz;
    vec3 random = texture(uNoiseTexture, coord * uScreenSize / 4.0f).xyz;
    vec3 tangent = normalize(random - normalVector * dot(random, normalVector));
    vec3 bitangent = cross(normalVector, tangent);
    mat3 TBN = mat3(tangent, bitangent, normalVector);

    const int kernelSize = 64;
    float viewSpacePositionDepth = (uViewMatrix * vec4(position, 1)).z;

    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; i++) {
        vec3 samplePosition = TBN * kernels[i].xyz;
        samplePosition = position + samplePosition * uRadius;
        samplePosition = (uViewMatrix * vec4(samplePosition, 1)).xyz;

        vec4 offset = uProjectionMatrix * vec4(samplePosition, 1);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        vec3 offsetPosition = PositionFromDepth(
            uProjectionMatrix * uViewMatrix,
            offset.xy, texture(depth, offset.xy).r
        );
        float sampleDepth = (uViewMatrix * vec4(offsetPosition, 1)).z;

        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(viewSpacePositionDepth - sampleDepth));
        const float bias = 0.025;
        occlusion += (sampleDepth >= samplePosition.z + bias ? 1.0 : 0.0);  
    }

    fragColor = 1.0 - (occlusion / kernelSize);
}
