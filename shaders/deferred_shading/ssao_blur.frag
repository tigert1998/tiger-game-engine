#version 330 core
out float fragColor;

uniform vec2 uScreenSize;
uniform sampler2D uInput;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec2 texelSize = 1.0 / vec2(textureSize(uInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(uInput, coord + offset).r;
        }
    }
    fragColor = result / (4.0 * 4.0);
}