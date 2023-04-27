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

std::string LightSources::kFsSource = R"(
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

vec3 calcAmbient(vec3 ka) {
    return ka;
}

vec3 calcDiffuse(vec3 lightDirection, vec3 normal, vec3 kd) {
    float diffuse = max(dot(normalize(lightDirection), normalize(normal)), 0);
    return diffuse * kd;
}

vec3 calcSpecular(vec3 lightDirection, vec3 normal, vec3 viewDirection, float shininess, vec3 ks) {
    float specular = dot(reflect(normalize(-lightDirection), normalize(normal)), normalize(viewDirection));
    specular = max(specular, 0);
    specular = pow(specular, shininess);
    return specular * ks;
}

vec3 calcPhongLighting(
    vec3 ka, vec3 kd, vec3 ks,
    vec3 normal, vec3 cameraPosition, vec3 position,
    float shininess
) {
    vec3 color = vec3(0);
    for (int i = 0; i < uAmbientLightCount; i++) {
        color += calcAmbient(ka) * uAmbientLights[i].color;
    }
    for (int i = 0; i < uDirectionalLightCount; i++) {
        color += calcDiffuse(-uDirectionalLights[i].dir, normal, kd) *
            uDirectionalLights[i].color;
        color += calcSpecular(-uDirectionalLights[i].dir, normal, cameraPosition - position, shininess, ks) *
            uDirectionalLights[i].color;
    }
    for (int i = 0; i < uPointLightCount; i++) {
        vec3 attenuation = uPointLights[i].attenuation;
        float dis = distance(uPointLights[i].pos, position);
        vec3 pointLightColor = uPointLights[i].color /
            (attenuation.x + attenuation.y * dis + attenuation.z * pow(dis, 2));
        color += calcDiffuse(uPointLights[i].pos - position, normal, kd) *
            pointLightColor;
        color += calcSpecular(uPointLights[i].pos - position, normal, cameraPosition - position, shininess, ks) *
            pointLightColor;
    }
    return color;
}
)";