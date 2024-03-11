#ifndef VXGI_VXGI_GLSL_
#define VXGI_VXGI_GLSL_

#include "light_sources.glsl"
#include "common/vxgi_accumulation.glsl"

const vec3 CONE_DIRECTIONS[6] = vec3[]
( 
    vec3(0, 0, 1),
    vec3(0, 0.866025,0.5),
    vec3(0.823639, 0.267617, 0.5),
    vec3(0.509037, -0.700629, 0.5),
    vec3(-0.509037, -0.700629, 0.5),
    vec3(-0.823639, 0.267617, 0.5)
);

const float CONE_WEIGHTS[6] = float[](0.25, 0.15, 0.15, 0.15, 0.15, 0.15);

vec4 sampleVoxel(vec3 position, float diameter, uint voxelResolution, float worldSize, sampler3D radianceMap) {
    float level = log2(max(diameter / worldSize * voxelResolution, 1));
    return textureLod(radianceMap, position / worldSize + 0.5, min(level, 6));
}

vec4 ConeTracing(
    vec3 origin, vec3 normal, vec3 direction, float aperture,
    float stepSize, float offset, float maxT,
    uint voxelResolution, float worldSize, sampler3D radianceMap
) {
    float voxelCellSize = worldSize / voxelResolution;
    float t = offset * voxelCellSize;
    vec4 color = vec4(0); // color and occlusion
    float diameter = 2 * t * tan(aperture / 2);
    origin += normal * voxelCellSize;
    vec3 position = origin + t * direction;

    while (color.a < 1 && t < maxT) {
        vec4 sampled = sampleVoxel(position, diameter, voxelResolution, worldSize, radianceMap);
        color = Accumulate(color, sampled);
        t += stepSize * diameter;
        diameter = 2 * t * tan(aperture / 2);
        position = origin + t * direction;
    }

    return color;
}

struct VXGIConfig {
    float stepSize;
    float diffuseOffset;
    float diffuseMaxT;
    float specularAperture;
    float specularOffset;
    float specularMaxT;
    uint voxelResolution;
    float worldSize;
    sampler3D radianceMap;
};

vec3 VXGI(
    vec3 position, vec3 directLighting,

    vec3 albedo, float metallic, float roughness,
    vec3 normal, vec3 viewDirection,

    VXGIConfig config
) {
    normal = normalize(normal);
    viewDirection = normalize(viewDirection);
    vec3 up = (abs(normal.y) > 1 - 1e-6) ? vec3(1, 0, 0) : vec3(0, 1, 0);
    vec3 T = normalize(cross(up, normal));
    vec3 B = normalize(cross(normal, T));
    mat3 TBN = mat3(T, B, normal);
    const float PI = radians(180);

    vec3 indirectDiffuse = vec3(0);

    for (int i = 0; i < 6; i++) {
        vec3 lightDirection = TBN * CONE_DIRECTIONS[i];
        vec3 halfway = normalize(viewDirection + lightDirection);

        vec3 f0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlick(halfway, viewDirection, f0);
        vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);

        indirectDiffuse += ConeTracing(
            position, TBN[2], lightDirection, radians(60),
            config.stepSize, config.diffuseOffset, config.diffuseMaxT,
            config.voxelResolution, config.worldSize, config.radianceMap
        ).rgb * CONE_WEIGHTS[i] * albedo / PI * kd;
    }

    vec3 indirectSpecular = vec3(0);

    {
        vec3 lightDirection = normalize(reflect(-viewDirection, normal));
        vec3 halfway = normalize(viewDirection + lightDirection);

        vec3 f0 = mix(vec3(0.04), albedo, metallic);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, halfway, roughness);
        float G = GeometrySmith(normal, viewDirection, lightDirection, roughness);
        vec3 F = FresnelSchlick(halfway, viewDirection, f0);

        vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);

        float nDotL = max(dot(normal, lightDirection), 0.0);
        float nDotV = max(dot(normal, viewDirection), 0.0);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * nDotV * nDotL + 1e-6;
        vec3 specular = numerator / denominator;

        indirectSpecular = ConeTracing(
            position, TBN[2], lightDirection, config.specularAperture,
            config.stepSize, config.specularOffset, config.specularMaxT,
            config.voxelResolution, config.worldSize, config.radianceMap
        ).rgb * specular * nDotL;
    }

    return directLighting + indirectDiffuse + indirectSpecular;
}

#endif
