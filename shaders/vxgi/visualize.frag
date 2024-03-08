#version 460 core

uniform vec3 uCameraPosition;
uniform usampler3D uVoxel;
uniform sampler2D uFront;
uniform sampler2D uBack;
uniform uint uMipmapLevel;
uniform vec4 uViewport;

const float STEP_SIZE = 0.003;

out vec4 fragColor;

#include "common/tone_mapping_and_gamma_correction.glsl"

bool IsInsideCube(vec3 position, float eps) {
    return abs(position.x) < 1 + eps && abs(position.y) < 1 + eps && abs(position.z) < 1 + eps;
}

vec4 ConvertRGBA8ToVec4(uint val){
    return vec4(
        float((val & 0x000000FF)),
        float((val & 0x0000FF00) >> 8U),
        float((val & 0x00FF0000) >> 16U),
        float((val & 0xFF000000) >> 24U)
    );
}

void main() {
    vec2 texCoord = gl_FragCoord.xy / uViewport.zw;
    vec3 origin = IsInsideCube(uCameraPosition, 0.2) ? uCameraPosition : texture2D(uBack, texCoord).xyz;
    vec3 end = texture2D(uFront, texCoord).xyz;
    vec3 direction = normalize(end - origin);

    vec4 color = vec4(0);
    int numSteps = int(length(end - origin) / STEP_SIZE);
    for (int i = 0; i < numSteps; i++) {
        vec3 position = origin + direction * STEP_SIZE * i;
        vec4 sampled = ConvertRGBA8ToVec4(textureLod(uVoxel, position * 0.5 + 0.5, uMipmapLevel).r);

        vec4 voxel = vec4(sampled.rgb / 255.0, float(sampled.a >= 1));
        color += (1 - color.a) * voxel;
        if (color.a >= 0.95) break;
    }

    fragColor = vec4(GammaCorrection(color.rgb), 1);
}
