#version 410 core

out vec4 fragColor;

in vec3 vTexCoord;

uniform samplerCube uSkyboxTexture;

void main() {
    fragColor = vec4(texture(uSkyboxTexture, vTexCoord).rgb, 1);
}
