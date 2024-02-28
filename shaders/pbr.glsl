#ifndef PBR_GLSL_
#define PBR_GLSL_

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

#endif
