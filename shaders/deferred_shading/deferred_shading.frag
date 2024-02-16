#version 460 core

#extension GL_ARB_bindless_texture : require

uniform mat4 uViewMatrix;
uniform vec3 uCameraPosition;

#include "light_sources.glsl"
#include "shadow_sources.glsl"

out vec4 fragColor;

uniform sampler2D ka;
uniform sampler2D kd;
uniform sampler2D ksAndShininess;
uniform sampler2D albedo;
uniform sampler2D metallicAndRoughnessAndAo;
uniform sampler2D normal;
uniform sampler2D positionAndAlpha;
uniform usampler2D flag;
uniform sampler2D depth;
uniform sampler2D uSSAO;

uniform vec2 uScreenSize;
uniform bool uEnableSSAO;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    gl_FragDepth = texture(depth, coord).r;

    uint renderType = texture(flag, coord).r; 
    if (renderType == 0) {
        discard;
    } else if (renderType == 1) {
        float shadow = CalcShadow(
            texture(positionAndAlpha, coord).xyz, 
            texture(normal, coord).xyz
        );

        fragColor.rgb = CalcPhongLighting(
            texture(ka, coord).rgb,
            texture(kd, coord).rgb,
            texture(ksAndShininess, coord).rgb,
            texture(normal, coord).xyz,
            uCameraPosition,
            texture(positionAndAlpha, coord).xyz,
            texture(ksAndShininess, coord).w,
            shadow
        );
    } else if (renderType == 2) {
        float shadow = CalcShadow(
            texture(positionAndAlpha, coord).xyz,
            texture(normal, coord).xyz
        );

        float ao = texture(metallicAndRoughnessAndAo, coord).z;
        if (uEnableSSAO) {
            ao = texture(uSSAO, coord).r;
        }

        fragColor.rgb = CalcPBRLighting(
            texture(albedo, coord).rgb, 
            texture(metallicAndRoughnessAndAo, coord).x, 
            texture(metallicAndRoughnessAndAo, coord).y,
            ao,
            texture(normal, coord).xyz,
            uCameraPosition,
            texture(positionAndAlpha, coord).xyz,
            shadow
        );
    }

    fragColor.a = 1;
}
