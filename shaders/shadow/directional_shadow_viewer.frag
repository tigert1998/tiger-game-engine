#version 460

uniform sampler2DArray uDirectionalShadowMap;
uniform vec4 uViewport;
uniform uint uLayer;

out vec4 fragColor;

void main() {
    vec2 uv = (gl_FragCoord.xy - uViewport.xy) / min(uViewport.z, uViewport.w);
    float sampled = texture(uDirectionalShadowMap, vec3(uv, uLayer)).r;
    fragColor = vec4(vec3(sampled), 1);
}
