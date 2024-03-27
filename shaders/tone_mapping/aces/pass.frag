#version 460 core

vec3 ACESToneMapping(vec3 color, float adaptedLum) {
    color = min(color, vec3(1e9));

    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;

    color *= adaptedLum;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

out vec4 fragColor;

uniform sampler2D uScene;
uniform vec4 uViewport;
uniform float uAdaptedLum;

#include "common/gamma_correction.glsl"

void main() {
    vec2 texCoords = gl_FragCoord.xy / uViewport.zw;
    vec3 color = texture2D(uScene, texCoords).rgb;
    color = GammaCorrection(ACESToneMapping(color, uAdaptedLum));
    fragColor = vec4(color, 1);
}
