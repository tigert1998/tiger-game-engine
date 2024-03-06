#version 460 core

out vec4 fragColor;

uniform sampler2D uScene;
uniform float uAdaptedLum;
uniform vec4 uViewport;

#include "common/tone_mapping_and_gamma_correction.glsl"

void main() {
    vec2 texCoords = gl_FragCoord.xy / uViewport.zw;
    vec3 color = texture2D(uScene, texCoords).rgb;
    color = GammaCorrection(ACESToneMapping(color, uAdaptedLum));
    fragColor = vec4(color, 1);
}
