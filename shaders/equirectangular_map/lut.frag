#version 460 core

#include "pbr.glsl"
#include "equirectangular_map/importance_sample_ggx.glsl"

uniform vec4 uViewport;
out vec2 fragColor;

vec2 IntegrateBRDF(float nDotV, float roughness) {
    vec3 viewDirection;
    viewDirection.x = sqrt(1.0 - nDotV * nDotV);
    viewDirection.y = 0.0;
    viewDirection.z = nDotV;

    float scale = 0.0;
    float bias = 0.0;

    vec3 normal = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024;
    for(uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 halfway = ImportanceSampleGGX(xi, normal, roughness);
        vec3 lightDirection = normalize(2.0 * dot(viewDirection, halfway) * halfway - viewDirection);

        float nDotL = max(lightDirection.z, 0.0);
        float nDotH = max(halfway.z, 0.0);
        float vDotH = max(dot(viewDirection, halfway), 0.0);

        if (nDotL > 0.0) {
            float G = GeometrySmithIBL(normal, viewDirection, lightDirection, roughness);
            float GVis = (G * vDotH) / (nDotH * nDotV);
            float fc = pow(1.0 - vDotH, 5.0);

            scale += (1.0 - fc) * GVis;
            bias += fc * GVis;
        }
    }
    scale /= float(SAMPLE_COUNT);
    bias /= float(SAMPLE_COUNT);
    return vec2(scale, bias);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - uViewport.xy) / vec2(uViewport.z, uViewport.w);
    fragColor = IntegrateBRDF(uv[0], uv[1]);
}
