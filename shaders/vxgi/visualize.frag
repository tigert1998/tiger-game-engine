#version 460 core

uniform vec3 uCameraPosition;
uniform uint uVoxelType;
uniform usampler3D uVoxelU;
uniform sampler3D uVoxelF;
uniform sampler2D uFront;
uniform sampler2D uBack;
uniform uint uMipmapLevel;
uniform vec4 uViewport;
uniform float uWorldSize;

const float STEP_SIZE = 0.003;

out vec4 fragColor;

#include "common/tone_mapping_and_gamma_correction.glsl"

bool IsInsideUnitCube(vec3 position, float eps) {
    return abs(position.x) < 1 + eps && abs(position.y) < 1 + eps && abs(position.z) < 1 + eps;
}

void main() {
    vec2 texCoord = gl_FragCoord.xy / uViewport.zw;
    vec3 origin = IsInsideUnitCube(uCameraPosition / (0.5 * uWorldSize), 0.2) ? 
        uCameraPosition : texture2D(uBack, texCoord).xyz;
    vec3 end = texture2D(uFront, texCoord).xyz;
    origin /= (0.5 * uWorldSize);
    end /= (0.5 * uWorldSize);
    vec3 direction = normalize(end - origin);

    vec4 color = vec4(0);
    int numSteps = int(length(end - origin) / STEP_SIZE);
    for (int i = 0; i < numSteps; i++) {
        vec3 position = origin + direction * STEP_SIZE * i;
        vec4 voxel;
        if (uVoxelType == 0) {
            vec4 sampled = unpackUnorm4x8(textureLod(uVoxelU, position * 0.5 + 0.5, uMipmapLevel).r);
            voxel = vec4(sampled.rgb, float(sampled.a > 0));
        } else if (uVoxelType == 1) {
            voxel = textureLod(uVoxelF, position * 0.5 + 0.5, uMipmapLevel);
        }

        color += (1 - color.a) * voxel;
        if (color.a >= 0.95) break;
    }

    fragColor = vec4(GammaCorrection(color.rgb), 1);
}
