#include "light_sources.h"

#include "cg_exception.h"

Ambient::Ambient(glm::vec3 color) : color_(color) {}

void Ambient::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uAmbientLights.n") && not_found_ok)
    return;
  int32_t id = shader->GetUniform<int32_t>("uAmbientLights.n");
  std::string var =
      std::string("uAmbientLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uAmbientLights.n", id + 1);
}

Directional::Directional(glm::vec3 dir, glm::vec3 color)
    : dir_(dir), color_(color) {}

void Directional::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uDirectionalLights.n") && not_found_ok)
    return;
  int32_t id = shader->GetUniform<int32_t>("uDirectionalLights.n");
  std::string var =
      std::string("uDirectionalLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".dir", dir_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uDirectionalLights.n", id + 1);
}

Point::Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation)
    : pos_(pos), color_(color), attenuation_(attenuation) {}

void Point::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uPointLights.n") && not_found_ok) return;
  int32_t id = shader->GetUniform<int32_t>("uPointLights.n");
  std::string var =
      std::string("uPointLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".pos", pos_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<glm::vec3>(var + ".attenuation", attenuation_);
  shader->SetUniform<int32_t>("uPointLights.n", id + 1);
}

void LightSources::Add(std::unique_ptr<Light> light) {
  lights_.emplace_back(std::move(light));
}

Light *LightSources::Get(int32_t index) { return lights_[index].get(); }

void LightSources::Set(Shader *shader, bool not_found_ok) {
  for (auto name :
       std::vector<std::string>{"Ambient", "Directional", "Point"}) {
    std::string var = std::string("u") + name + "Lights.n";
    if (!shader->UniformVariableExists(var) && not_found_ok) continue;
    shader->SetUniform<int32_t>(var, 0);
  }

  for (int i = 0; i < lights_.size(); i++) {
    lights_[i]->Set(shader, not_found_ok);
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

struct AmbientLights {
    int n;
    AmbientLight l[1];
};

struct DirectionalLights {
    int n;
    DirectionalLight l[1];
};

struct PointLights {
    int n;
    PointLight l[8];
};

uniform AmbientLights uAmbientLights;
uniform DirectionalLights uDirectionalLights;
uniform PointLights uPointLights;

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
    float shininess, float shadow
) {
    vec3 color = vec3(0);
    for (int i = 0; i < uAmbientLights.n; i++) {
        color += CalcAmbient(ka) * uAmbientLights.l[i].color;
    }
    for (int i = 0; i < uDirectionalLights.n; i++) {
        color += CalcDiffuse(-uDirectionalLights.l[i].dir, normal, kd) *
            uDirectionalLights.l[i].color * (1 - shadow);
        color += CalcSpecular(-uDirectionalLights.l[i].dir, normal, cameraPosition - position, shininess, ks) *
            uDirectionalLights.l[i].color * (1 - shadow);
    }
    for (int i = 0; i < uPointLights.n; i++) {
        vec3 attenuation = uPointLights.l[i].attenuation;
        float dis = distance(uPointLights.l[i].pos, position);
        vec3 pointLightColor = uPointLights.l[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcDiffuse(uPointLights.l[i].pos - position, normal, kd) *
            pointLightColor * (1 - shadow);
        color += CalcSpecular(uPointLights.l[i].pos - position, normal, cameraPosition - position, shininess, ks) *
            pointLightColor * (1 - shadow);
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
    vec3 normal, vec3 cameraPosition, vec3 position,
    float shadow
) {
    vec3 color = vec3(0);

    for (int i = 0; i < uAmbientLights.n; i++) {
        color += albedo * uAmbientLights.l[i].color * ao;
    }

    for (int i = 0; i < uDirectionalLights.n; i++) {
        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, -uDirectionalLights.l[i].dir,
            uDirectionalLights.l[i].color
        ) * (1 - shadow);
    }
    for (int i = 0; i < uPointLights.n; i++) {
        vec3 attenuation = uPointLights.l[i].attenuation;
        float dis = distance(uPointLights.l[i].pos, position);
        vec3 pointLightColor = uPointLights.l[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += CalcPBRLightingForSingleLightSource(
            albedo, metallic, roughness,
            normal, cameraPosition - position, uPointLights.l[i].pos - position,
            pointLightColor
        ) * (1 - shadow);
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}
)";
}