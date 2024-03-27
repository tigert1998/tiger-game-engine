#version 460 core

layout (rg16f) readonly uniform image3D uGrid;
layout (rgba16f) readonly uniform image2D uInput;

uniform uint uScaleSize;
uniform uint uScaleRange;
uniform float uAlpha;
uniform float uBeta;
uniform float uBlend;
uniform float uExposure;

#include "tone_mapping/bilateral_grid/common.glsl"
#include "common/gamma_correction.glsl"

out vec4 fragColor;

vec2 SampleGridSpatial(float x, float y, uint z) {
    vec2 a = mix(
        imageLoad(uGrid, ivec3(x, y, z)).rg, 
        imageLoad(uGrid, ivec3(x + 1, y, z)).rg,
        fract(x)
    );
    vec2 b = mix(
        imageLoad(uGrid, ivec3(x, y + 1, z)).rg, 
        imageLoad(uGrid, ivec3(x + 1, y + 1, z)).rg,
        fract(x)
    );
    return mix(a, b, fract(y));
}

vec2 SampleGrid(float x, float y, float z) {
    return mix(
        SampleGridSpatial(x, y, uint(z)),
        SampleGridSpatial(x, y, uint(z) + 1), fract(z)
    );
}

vec3 BilateralGridToneMappingPass4(
    ivec2 coord,
    uint scaleSize, uint scaleRange,
    float alpha, float beta, float blend, float exposure
) {
    vec3 c = imageLoad(uInput, coord).rgb / 1024.0;
    float l = LogLuminance(c);

    vec2 sampled = SampleGrid(
        float(coord.x) / scaleSize,
        float(coord.y) / scaleSize, 
        l / scaleRange
    );
    float base = sampled.x / sampled.y;
    float detail = l - base;
    float finalLogLum = alpha * base + beta + detail;
    float linearScale = pow(2, (finalLogLum - l) / LOG_LUMINANCE_SCALE);
    vec3 ldr = mix(c, c * linearScale, blend);
    ldr = min(vec3(1), ldr * pow(2, exposure));

    return ldr;
}

void main() {
    vec3 color = BilateralGridToneMappingPass4(
        ivec2(gl_FragCoord.xy), 
        uScaleSize, uScaleRange,
        uAlpha, uBeta, uBlend, uExposure
    );
    color = GammaCorrection(color);
    fragColor = vec4(color, 1);
}
