#version 460 core

out vec4 fragColor;

uniform sampler2D uScene;
uniform sampler2D uBloomBlur;
uniform vec4 uViewport;

const float BLOOM_STRENGTH = 0.04;

vec3 Bloom(vec2 texCoords) {
    vec3 hdrColor = texture(uScene, texCoords).rgb;
    vec3 bloomColor = texture(uBloomBlur, texCoords).rgb;
    return mix(hdrColor, bloomColor, BLOOM_STRENGTH); // linear interpolation
}

void main() {
    vec2 texCoords = gl_FragCoord.xy / uViewport.zw;
    fragColor = vec4(Bloom(texCoords), 1);
}
