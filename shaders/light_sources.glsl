#ifndef LIGHT_SOURCES_GLSL_
#define LIGHT_SOURCES_GLSL_

#include "shadow/shadows.glsl"

struct AmbientLight {
    vec3 color;
};

struct DirectionalLight {
    vec3 dir;
    vec3 color;
    bool shadowEnabled;
    DirectionalShadow shadow;
};

struct PointLight {
    vec3 pos;
    vec3 color;
    vec3 attenuation;
    bool shadowEnabled;
    OmnidirectionalShadow shadow;
};

layout (std430, binding = AMBIENT_LIGHT_BINDING) buffer ambientLightsBuffer {
    AmbientLight ambientLights[];
};

layout (std430, binding = DIRECTIONAL_LIGHT_BINDING) buffer directionalLightsBuffer {
    DirectionalLight directionalLights[];
};

layout (std430, binding = POINT_LIGHT_BINDING) buffer pointLightsBuffer {
    PointLight pointLights[];
};

vec3 CalcAmbient(vec3 ka) {
    return ka;
}

vec3 CalcDiffuse(vec3 lightDirection, vec3 normal, vec3 kd) {
    float diffuse = max(dot(normalize(lightDirection), normalize(normal)), 0);
    return diffuse * kd;
}

vec3 CalcSpecular(vec3 lightDirection, vec3 normal, vec3 viewDirection, float shininess, vec3 ks) {
    float specular = dot(reflect(normalize(-lightDirection), normalize(normal)), normalize(viewDirection));
    specular = max(specular, 0);
    specular = pow(specular, shininess);
    return specular * ks;
}

vec3 CalcPhongLighting(
    vec3 ka, vec3 kd, vec3 ks, vec3 emission,
    vec3 normal, vec3 cameraPosition, vec3 position,
    float shininess, mat4 cameraViewMatrix
) {
    vec3 color = emission;
    for (int i = 0; i < ambientLights.length(); i++) {
        color += CalcAmbient(ka) * ambientLights[i].color;
    }
    for (int i = 0; i < directionalLights.length(); i++) {
        float shadow = directionalLights[i].shadowEnabled ? 
            CalcDirectionalShadow(directionalLights[i].shadow, position, cameraViewMatrix) : 0;

        color += CalcDiffuse(-directionalLights[i].dir, normal, kd) *
            directionalLights[i].color * (1 - shadow);
        color += CalcSpecular(-directionalLights[i].dir, normal, cameraPosition - position, shininess, ks) *
            directionalLights[i].color * (1 - shadow);
    }
    for (int i = 0; i < pointLights.length(); i++) {
        float shadow = pointLights[i].shadowEnabled ? 
            CalcOmnidirectionalShadow(pointLights[i].shadow, position) : 0;

        vec3 attenuation = pointLights[i].attenuation;
        float dis = distance(pointLights[i].pos, position);
        vec3 pointLightColor = pointLights[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcDiffuse(pointLights[i].pos - position, normal, kd) *
            pointLightColor * (1 - shadow);
        color += CalcSpecular(pointLights[i].pos - position, normal, cameraPosition - position, shininess, ks) *
            pointLightColor * (1 - shadow);
    }

    // tone mapping
    color = color / (color + vec3(1.0));
    // gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}

float DistributionGGX(vec3 normal, vec3 halfway, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfway), 0.0);
    float nDotH2 = nDotH * nDotH;

    float num = a2;
    float denom = (nDotH2 * (a2 - 1.0) + 1.0);
    const float PI = radians(180);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(vec3 normal, vec3 viewDirection, float roughness) {
    float k = pow(roughness + 1.0, 2) / 8.0;

    float nDotV = max(dot(normal, viewDirection), 0.0);
    float num = nDotV;
    float denom = nDotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness) {
    float ggx2  = GeometrySchlickGGX(normal, viewDirection, roughness);
    float ggx1  = GeometrySchlickGGX(normal, lightDirection, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(vec3 halfway, vec3 viewDirection, vec3 f0) {
    float hDotV = max(dot(halfway, viewDirection), 0.0);
    return f0 + (1.0 - f0) * pow(clamp(1.0 - hDotV, 0.0, 1.0), 5.0);
}

vec3 CalcPBRLightingForSingleLightSource(
    vec3 albedo, float metallic, float roughness,
    vec3 normal, vec3 viewDirection, vec3 lightDirection,
    vec3 lightColor
) {
    normal = normalize(normal);
    viewDirection = normalize(viewDirection);
    lightDirection = normalize(lightDirection);
    vec3 halfway = normalize(viewDirection + lightDirection);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, halfway, roughness);
    float G = GeometrySmith(normal, viewDirection, lightDirection, roughness); 
    vec3 F = FresnelSchlick(halfway, viewDirection, f0);

    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, lightDirection), 0.0) + 1e-6;
    vec3 specular = numerator / denominator;

    const float PI = radians(180);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return (kd * albedo / PI + specular) * lightColor * nDotL;
}

vec3 CalcPBRLighting(
    vec3 albedo, float metallic, float roughness, float ao, vec3 emission,
    vec3 normal, vec3 cameraPosition, vec3 position, mat4 cameraViewMatrix
) {
    vec3 color = emission;

    for (int i = 0; i < ambientLights.length(); i++) {
        color += albedo * ambientLights[i].color * ao;
    }

    for (int i = 0; i < directionalLights.length(); i++) {
        float shadow = directionalLights[i].shadowEnabled ? 
            CalcDirectionalShadow(directionalLights[i].shadow, position, cameraViewMatrix) : 0;

        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, -directionalLights[i].dir,
            directionalLights[i].color
        ) * (1 - shadow);
    }
    for (int i = 0; i < pointLights.length(); i++) {
        float shadow = pointLights[i].shadowEnabled ? 
            CalcOmnidirectionalShadow(pointLights[i].shadow, position) : 0;

        vec3 attenuation = pointLights[i].attenuation;
        float dis = distance(pointLights[i].pos, position);
        vec3 pointLightColor = pointLights[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, pointLights[i].pos - position,
            pointLightColor
        ) * (1 - shadow);
    }

    // tone mapping
    color = color / (color + vec3(1.0));
    // gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}

#endif
