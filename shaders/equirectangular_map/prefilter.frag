#version 460 core

in vec3 vLocalPosition;

uniform samplerCube uTexture;
uniform float uRoughness;

out vec4 fragColor;

const float PI = radians(180);

#include "pbr.glsl"

float RadicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // 0x100000000
}

vec2 Hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, vec3 normal, float roughness) {
    float a = roughness * roughness;
	
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    // from spherical coordinates to cartesian coordinates
    vec3 h;
    h.x = cos(phi) * sinTheta;
    h.y = sin(phi) * sinTheta;
    h.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    vec3 up = abs(normal.z) < (1 - 1e-3) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    vec3 sampleVector = tangent * h.x + bitangent * h.y + normal * h.z;
    return normalize(sampleVector);
}

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
