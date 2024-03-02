#version 460 core

out vec4 fragColor;

in vec3 vLocalPosition;

uniform sampler2D uTexture;

const vec2 INV_ATAN = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= INV_ATAN;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(vLocalPosition));
    // make sure to normalize vLocalPosition
    vec3 color = texture(uTexture, uv).rgb;

    fragColor = vec4(color, 1.0);
}
