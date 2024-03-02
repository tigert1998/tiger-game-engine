#version 410 core

out vec4 fragColor;

in vec3 vTexCoord;

uniform samplerCube uSkyboxTexture;
uniform bool uToneMap;

void main() {
    fragColor.rgb = texture(uSkyboxTexture, vTexCoord).rgb;
    if (uToneMap) {
        fragColor.rgb = fragColor.rgb / (fragColor.rgb + vec3(1.0));
    }
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / 2.2));
    fragColor.a = 1;
}
