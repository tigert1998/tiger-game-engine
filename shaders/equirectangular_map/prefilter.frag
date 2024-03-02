#version 460 core

in vec3 vLocalPosition;

uniform samplerCube uTexture;
uniform float uRoughness;

out vec4 fragColor;

const float PI = radians(180);

#include "pbr.glsl"
#include "equirectangular_map/importance_sample_ggx.glsl"

void main() {
    float resolution = float(textureSize(uTexture, 0).x);
    float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);

    vec3 normal = normalize(vLocalPosition);    
    vec3 viewDirection = normal;

    const uint SAMPLE_COUNT = 1024;
    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);
    for (uint i = 0; i < SAMPLE_COUNT; i++) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 halfway = ImportanceSampleGGX(xi, normal, uRoughness);
        vec3 lightDirection = normalize(2.0 * dot(viewDirection, halfway) * halfway - viewDirection);

        float nDotL = max(dot(normal, lightDirection), 0.0);
        if(nDotL > 0.0) {
            float nDotH = max(dot(normal, halfway), 0);
            float hDotV = max(dot(halfway, viewDirection), 0);

            float D = DistributionGGX(normal, halfway, uRoughness);
            float pdf = (D * nDotH / (4.0 * hDotV)) + 1e-4; 

            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 1e-4);

            float mipLevel = uRoughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 

            prefilteredColor += textureLod(uTexture, lightDirection, mipLevel).rgb * nDotL;
            totalWeight += nDotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    fragColor = vec4(prefilteredColor, 1.0);
}
