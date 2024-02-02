#version 410 core

uniform sampler2D uScreenTexture;
uniform sampler2D uCloudTexture;
uniform sampler2D uDepthTexture;
uniform vec2 uScreenSize;

out vec4 fragColor;

void main() {
    vec2 texCoord = gl_FragCoord.xy / uScreenSize;

    vec4 cloud = texture(uCloudTexture, texCoord);
    vec4 background = texture(uScreenTexture, texCoord);
    float a = texture(uDepthTexture, texCoord).r < 1.0 ? 0.0 : 1.0;
    fragColor = mix(background, cloud, a);
}
