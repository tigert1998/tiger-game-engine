#version 460 core

#extension GL_ARB_bindless_texture : require

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform vec3 uCameraPosition;

#include "light_sources.glsl"
#include "common/position_from_depth.glsl"

out vec4 fragColor;

uniform sampler2D ka;
uniform sampler2D kd;
uniform sampler2D ksAndShininess;
uniform sampler2D albedo;
uniform sampler2D metallicAndRoughnessAndAo;
uniform sampler2D normalAndAlpha;
uniform usampler2D flag;
uniform sampler2D depth;
uniform sampler2D uSSAO;

uniform vec2 uScreenSize;
uniform bool uEnableSSAO;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    gl_FragDepth = texture(depth, coord).r;
    vec3 position = PositionFromDepth(uProjectionMatrix * uViewMatrix, coord, gl_FragDepth);

    uint renderType = texture(flag, coord).r; 
    if (renderType == 0) {
        discard;
    } else if (renderType == 1) {
        fragColor.rgb = CalcPhongLighting(
            texture(ka, coord).rgb,
            texture(kd, coord).rgb,
            texture(ksAndShininess, coord).rgb,
            texture(normalAndAlpha, coord).xyz,
            uCameraPosition,
            position,
            texture(ksAndShininess, coord).w,
            uViewMatrix
        );
    } else if (renderType == 2) {
        float ao = texture(metallicAndRoughnessAndAo, coord).z;
        if (uEnableSSAO) {
            ao = texture(uSSAO, coord).r;
        }

        fragColor.rgb = CalcPBRLighting(
            texture(albedo, coord).rgb, 
            texture(metallicAndRoughnessAndAo, coord).x, 
            texture(metallicAndRoughnessAndAo, coord).y,
            ao,
            texture(normalAndAlpha, coord).xyz,
            uCameraPosition,
            position,
            uViewMatrix
        );
    }

    fragColor.a = 1;
}
