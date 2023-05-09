#include "light_sources.h"

Ambient::Ambient(glm::vec3 color) : color_(color) {}

void Ambient::Set(Shader *shader) {
  int32_t id = shader->GetUniform<int32_t>("uAmbientLightCount");
  std::string var =
      std::string("uAmbientLights") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uAmbientLightCount", id + 1);
}

Directional::Directional(glm::vec3 dir, glm::vec3 color)
    : dir_(dir), color_(color) {}

void Directional::Set(Shader *shader) {
  int32_t id = shader->GetUniform<int32_t>("uDirectionalLightCount");
  std::string var =
      std::string("uDirectionalLights") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".dir", dir_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uDirectionalLightCount", id + 1);
}

Point::Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation)
    : pos_(pos), color_(color), attenuation_(attenuation) {}

void Point::Set(Shader *shader) {
  int32_t id = shader->GetUniform<int32_t>("uPointLightCount");
  std::string var =
      std::string("uPointLights") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".pos", pos_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<glm::vec3>(var + ".attenuation", attenuation_);
  shader->SetUniform<int32_t>("uPointLightCount", id + 1);
}

void LightSources::Add(std::unique_ptr<Light> light) {
  lights_.emplace_back(std::move(light));
}

void LightSources::Set(Shader *shader) {
  for (auto name :
       std::vector<std::string>{"Ambient", "Directional", "Point"}) {
    shader->SetUniform<int32_t>(std::string("u") + name + "LightCount", 0);
  }

  for (int i = 0; i < lights_.size(); i++) {
    lights_[i]->Set(shader);
  }
}

std::string LightSources::FsSource() {
  return R"(
struct AmbientLight {
    vec3 color;
};

struct DirectionalLight {
    vec3 dir;
    vec3 color;
};

struct PointLight {
    vec3 pos;
    vec3 color;
    vec3 attenuation;
};

#define REG_LIGHT(name, count) \
    uniform int u##name##LightCount;      \
    uniform name##Light u##name##Lights[count];

REG_LIGHT(Ambient, 1)
REG_LIGHT(Directional, 1)
REG_LIGHT(Point, 8)

#undef REG_LIGHT

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
    vec3 ka, vec3 kd, vec3 ks,
    vec3 normal, vec3 cameraPosition, vec3 position,
    float shininess
) {
    vec3 color = vec3(0);
    for (int i = 0; i < uAmbientLightCount; i++) {
        color += CalcAmbient(ka) * uAmbientLights[i].color;
    }
    for (int i = 0; i < uDirectionalLightCount; i++) {
        color += CalcDiffuse(-uDirectionalLights[i].dir, normal, kd) *
            uDirectionalLights[i].color;
        color += CalcSpecular(-uDirectionalLights[i].dir, normal, cameraPosition - position, shininess, ks) *
            uDirectionalLights[i].color;
    }
    for (int i = 0; i < uPointLightCount; i++) {
        vec3 attenuation = uPointLights[i].attenuation;
        float dis = distance(uPointLights[i].pos, position);
        vec3 pointLightColor = uPointLights[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcDiffuse(uPointLights[i].pos - position, normal, kd) *
            pointLightColor;
        color += CalcSpecular(uPointLights[i].pos - position, normal, cameraPosition - position, shininess, ks) *
            pointLightColor;
    }
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
    vec3 albedo, float metallic, float roughness, float ao,
    vec3 normal, vec3 cameraPosition, vec3 position
) {
    vec3 color = vec3(0);

    for (int i = 0; i < uAmbientLightCount; i++) {
        color += albedo * uAmbientLights[i].color * ao;
    }

    for (int i = 0; i < uDirectionalLightCount; i++) {
        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, -uDirectionalLights[i].dir,
            uDirectionalLights[i].color
        );
    }
    for (int i = 0; i < uPointLightCount; i++) {
        vec3 attenuation = uPointLights[i].attenuation;
        float dis = distance(uPointLights[i].pos, position);
        vec3 pointLightColor = uPointLights[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, uPointLights[i].pos - position,
            pointLightColor
        );
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}
)";
}