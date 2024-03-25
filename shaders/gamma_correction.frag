#version 460 core

out vec4 fragColor;

uniform sampler2D uScene;
uniform vec4 uViewport;

#include "common/gamma_correction.glsl"

void main() {
    vec2 texCoords = gl_FragCoord.xy / uViewport.zw;
    vec3 color = texture2D(uScene, texCoords).rgb;
    color = GammaCorrection(color);
    fragColor = vec4(color, 1);
}
