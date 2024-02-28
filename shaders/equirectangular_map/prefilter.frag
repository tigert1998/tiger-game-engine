#version 460 core

in vec3 vLocalPosition;

uniform samplerCube uTexture;
uniform float uRoughness;

out vec4 fragColor;

const float PI = radians(180);

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
            prefilteredColor += texture(uTexture, lightDirection).rgb * nDotL;
            totalWeight += nDotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    fragColor = vec4(prefilteredColor, 1.0);
}
