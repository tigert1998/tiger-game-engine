#version 460 core

#extension GL_ARB_bindless_texture : require

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform vec3 uCameraPosition;

#include "light_sources.glsl"
#include "common/position_from_depth.glsl"
#include "vxgi/vxgi.glsl"

out vec4 fragColor;

// G-Buffer
uniform sampler2D ka;
uniform sampler2D kd;
uniform sampler2D ksAndShininess;
uniform sampler2D albedo;
uniform sampler2D metallicAndRoughnessAndAo;
uniform sampler2D emission;
uniform sampler2D normalAndAlpha;
uniform usampler2D flag;
uniform sampler2D depth;
uniform sampler2D uSSAO;

uniform vec2 uScreenSize;
uniform bool uEnableSSAO;

uniform VXGIConfig uVXGIConfig;

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
            texture(emission, coord).rgb,
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

        vec3 sampled = texture(metallicAndRoughnessAndAo, coord).xyz;
        float metallic = sampled.x;
        float roughness = sampled.y;
        vec3 normal = texture(normalAndAlpha, coord).xyz;

        vec3 directLighting = CalcPBRLighting(
            texture(albedo, coord).rgb, 
            metallic, 
            roughness,
            ao,
            texture(emission, coord).rgb,
            normal,
            uCameraPosition,
            position,
            uViewMatrix
        );

        if (uVXGIConfig.on) {
            fragColor.rgb = VXGI(
                position, directLighting,
                texture(albedo, coord).rgb, metallic, roughness,
                normal, uCameraPosition - position,
                uVXGIConfig);
        } else {
            fragColor.rgb = directLighting;
        }
    }

    fragColor.a = 1;
}
