// clang-format off
#include <glad/glad.h>
// clang-format on

#include "shadow_sources.h"

#include <glm/gtc/matrix_transform.hpp>

DirectionalShadow::DirectionalShadow(glm::vec3 position, glm::vec3 direction,
                                     float width, float height, float z_near,
                                     float z_far, uint32_t fbo_width,
                                     uint32_t fbo_height)
    : position_(position),
      direction_(direction),
      width_(width),
      height_(height),
      near_(z_near),
      far_(z_far),
      fbo_width_(fbo_width),
      fbo_height_(fbo_height),
      fbo_(fbo_width, fbo_height, false) {}

void DirectionalShadow::Set(Shader *shader, int32_t *num_samplers) {
  int32_t id = shader->GetUniform<int32_t>("uDirectionalShadows.n");
  shader->SetUniformSampler2D(
      std::string("uDirectionalShadows.t") + "[" + std::to_string(id) + "]",
      fbo_.depth_texture_id(), (*num_samplers)++);
  shader->SetUniform<glm::vec3>(
      std::string("uDirectionalShadows.dirs") + "[" + std::to_string(id) + "]",
      direction_);
  shader->SetUniform<glm::mat4>(
      std::string("uDirectionalShadowViewProjectionMatrices[") +
          std::to_string(id) + "]",
      projection_matrix() * view_matrix());
  shader->SetUniform<int32_t>("uDirectionalShadows.n", id + 1);
}

void DirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_.Bind();
}

glm::mat4 DirectionalShadow::view_matrix() const {
  auto up = glm::vec3(0, 1, 0);
  if (std::abs(std::abs(glm::dot(up, glm::normalize(direction_))) - 1) <
      1e-8f) {
    up = glm::vec3(0, 0, 1);
  }
  return glm::lookAt(position_, position_ + direction_, up);
}

glm::mat4 DirectionalShadow::projection_matrix() const {
  return glm::ortho(-width_ / 2, width_ / 2, -height_ / 2, height_ / 2, near_,
                    far_);
}

void DirectionalShadow::SetForDepthPass(Shader *shader) {
  shader->SetUniform<glm::mat4>("uViewMatrix", view_matrix());
  shader->SetUniform<glm::mat4>("uProjectionMatrix", projection_matrix());
}

void ShadowSources::Add(std::unique_ptr<Shadow> shadow) {
  shadows_.emplace_back(std::move(shadow));
}

Shadow *ShadowSources::Get(int32_t index) { return shadows_[index].get(); }

void ShadowSources::Set(Shader *shader, int32_t *num_samplers) {
  for (auto name : {"Directional"}) {
    std::string var = std::string("u") + name + "Shadows.n";
    shader->SetUniform<int32_t>(var, 0);
  }

  for (int i = 0; i < shadows_.size(); i++) {
    shadows_[i]->Set(shader, num_samplers);
  }
}

void ShadowSources::DrawDepthForShadow(
    const std::function<void(Shadow *)> &render_pass) {
  glDisable(GL_CULL_FACE);
  for (int i = 0; i < shadows_.size(); i++) {
    shadows_[i]->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    render_pass(shadows_[i].get());
    shadows_[i]->Unbind();
  }
}

std::string ShadowSources::FsSource() {
  return R"(
const int MAX_DIRECTIONAL_SHADOWS = 1;

struct DirectionalShadows {
    int n;
    sampler2D t[MAX_DIRECTIONAL_SHADOWS];
    vec3 dirs[MAX_DIRECTIONAL_SHADOWS];
};

uniform DirectionalShadows uDirectionalShadows;

float CalcShadow(
    vec4 homoPositions[MAX_DIRECTIONAL_SHADOWS], vec3 normal
) {
    const float kShadowBiasFactor = 1e-3; 

    // do not take shadow into considerations
    if (uDirectionalShadows.n == 0) {
        return 0;
    }

    for (int i = 0; i < uDirectionalShadows.n; i++) {
        vec3 position = homoPositions[i].xyz / homoPositions[i].w;
        position = position * 0.5 + 0.5;
        float closestDepth = texture(uDirectionalShadows.t[i], position.xy).r;
        float currentDepth = position.z;
        float bias = max(kShadowBiasFactor * (1.0 - dot(normalize(normal), normalize(-uDirectionalShadows.dirs[i]))), kShadowBiasFactor * 1e-1);
        if (currentDepth - bias <= closestDepth) return 0;
    }

    return 1;
}
)";
}