#version 410 core

out vec4 fragColor;

in vec3 vTexCoord;

uniform samplerCube uSkyboxTexture;

void main() {
    fragColor = texture(uSkyboxTexture, vTexCoord);
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / 2.2));
}
