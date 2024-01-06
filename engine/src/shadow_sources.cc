#define NOMINMAX

// clang-format off
#include <glad/glad.h>
// clang-format on

#include "shadow_sources.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include "utils.h"

DirectionalShadow::DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                                     uint32_t fbo_height, const Camera *camera)
    : direction_(direction),
      fbo_width_(fbo_width),
      fbo_height_(fbo_height),
      camera_(camera) {
  Texture depth_texture(nullptr, GL_TEXTURE_2D_ARRAY, fbo_width, fbo_height,
                        NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                        GL_FLOAT, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR,
                        {1, 1, 1, 1}, false);
  fbo_.reset(new FrameBufferObject(std::vector<Texture>{}, depth_texture));
}

std::vector<glm::mat4> DirectionalShadow::view_projection_matrices() const {
  std::vector<glm::mat4> matrices;
  matrices.reserve(NUM_CASCADES);
  auto distances = cascade_plane_distances();

  for (int i = 0; i < NUM_CASCADES; i++) {
    double z_near, z_far;
    if (i == 0) {
      z_near = camera_->z_near();
      z_far = distances[i];
    } else if (0 < i && i < NUM_CASCADES - 1) {
      z_near = distances[i - 1];
      z_far = distances[i];
    } else if (i == NUM_CASCADES - 1) {
      z_near = distances[i - 1];
      z_far = camera_->z_far();
    }
    auto corners = camera_->frustum_corners(z_near, z_far);

    matrices.push_back(projection_matrix(corners) * view_matrix(corners));
  }

  return matrices;
}

void DirectionalShadow::Set(Shader *shader, int32_t *num_samplers) {
  shader->SetUniform<int32_t>("uDirectionalShadow.enabled", 1);
  shader->SetUniform<std::vector<glm::mat4>>(
      "uDirectionalShadow.viewProjectionMatrices", view_projection_matrices());
  shader->SetUniform<std::vector<float>>(
      "uDirectionalShadow.cascadePlaneDistances", cascade_plane_distances());
  shader->SetUniform<float>("uDirectionalShadow.farPlaneDistance",
                            camera_->z_far());
  shader->SetUniformSampler("uDirectionalShadow.shadowMap",
                            fbo_->depth_texture(), (*num_samplers)++);
  shader->SetUniform<glm::vec3>(std::string("uDirectionalShadow.dir"),
                                direction_);
}

void DirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_->Bind();
}

glm::mat4 DirectionalShadow::view_matrix(
    const std::vector<glm::vec3> &frustum_corners) const {
  auto up = glm::vec3(0, 1, 0);
  if (std::abs(std::abs(glm::dot(up, glm::normalize(direction_))) - 1) <
      1e-8f) {
    up = glm::vec3(0, 0, 1);
  }
  glm::vec3 center = glm::vec3(0);
  for (auto corner : frustum_corners) {
    center += corner;
  }
  center /= frustum_corners.size();
  return glm::lookAt(center - direction_, center, up);
}

glm::mat4 DirectionalShadow::projection_matrix(
    const std::vector<glm::vec3> &frustum_corners) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  for (auto corner : frustum_corners) {
    auto corner_view_space =
        view_matrix(frustum_corners) * glm::vec4(corner, 1);
    min_x = std::min(min_x, corner_view_space.x);
    max_x = std::max(max_x, corner_view_space.x);
    min_y = std::min(min_y, corner_view_space.y);
    max_y = std::max(max_y, corner_view_space.y);
    min_z = std::min(min_z, corner_view_space.z);
    max_z = std::max(max_z, corner_view_space.z);
  }

  // tune this parameter according to the scene
  constexpr float MULT = 10.0f;
  if (min_z < 0) {
    min_z *= MULT;
  } else {
    min_z /= MULT;
  }
  if (max_z < 0) {
    max_z /= MULT;
  } else {
    max_z *= MULT;
  }

  return glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
}

void DirectionalShadow::SetForDepthPass(Shader *shader) {
  shader->SetUniform<std::vector<glm::mat4>>(
      "uDirectionalShadowViewProjectionMatrices", view_projection_matrices());
}

void DirectionalShadow::ImGuiWindow(
    uint32_t index, const std::function<void()> &erase_callback) {
  ImGui::Text("Shadow Source #%d Type: Directional", index);
  if (ImGui::Button(
          ("Erase##shadow_source_" + std::to_string(index)).c_str())) {
    erase_callback();
  }
  float d_arr[3] = {direction_.x, direction_.y, direction_.z};
  ImGui::InputFloat3(
      ("Direction##shadow_source_" + std::to_string(index)).c_str(), d_arr);
  direction_ = glm::vec3(d_arr[0], d_arr[1], d_arr[2]);
}

void ShadowSources::Add(std::unique_ptr<Shadow> shadow) {
  shadows_.emplace_back(std::move(shadow));
}

Shadow *ShadowSources::Get(int32_t index) { return shadows_[index].get(); }

void ShadowSources::Set(Shader *shader, int32_t *num_samplers) {
  shader->SetUniform<int32_t>("uDirectionalShadow.enabled", 0);

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

void ShadowSources::ImGuiWindow() {
  ImGui::Begin("Shadow Sources:");

  const char *shadow_source_types[] = {"Directional"};
  static int shadow_source_type = 0;
  ImGui::ListBox("##add_shadow_source", &shadow_source_type,
                 shadow_source_types, IM_ARRAYSIZE(shadow_source_types));
  ImGui::SameLine();
  if (ImGui::Button("Add##add_shadow_source")) {
    if (shadow_source_type == 0) {
      Add(std::make_unique<DirectionalShadow>(glm::vec3(0, -1, 0), 2048, 2048,
                                              camera_));
    }
  }

  for (int i = 0; i < shadows_.size(); i++) {
    shadows_[i]->ImGuiWindow(
        i, [this, i]() { this->shadows_.erase(shadows_.begin() + i); });
  }
  ImGui::End();
}

std::string ShadowSources::FsSource() {
  return R"(
const int NUM_CASCADES = <!--NUM_CASCADES-->;

struct DirectionalShadow {
    // Cascaded Shadow Mapping
    bool enabled;

    mat4 viewProjectionMatrices[NUM_CASCADES];
    float cascadePlaneDistances[NUM_CASCADES - 1];
    float farPlaneDistance;
    sampler2DArray shadowMap;
    vec3 dir;
};

uniform DirectionalShadow uDirectionalShadow;

float CalcShadow(vec3 position, vec3 normal) {
    if (!uDirectionalShadow.enabled) {
        return 0;
    }

    // select cascade layer
    vec4 viewSpacePosition = uViewMatrix * vec4(position, 1);
    float depth = -viewSpacePosition.z;

    int layer = NUM_CASCADES - 1;
    for (int i = 0; i < NUM_CASCADES - 1; i++) {
        if (depth < uDirectionalShadow.cascadePlaneDistances[i]) {
            layer = i;
            break;
        }
    }

    vec4 homoPosition = uDirectionalShadow.viewProjectionMatrices[layer] * vec4(position, 1.0);

    const float kShadowBiasFactor = 1e-3;

    position = homoPosition.xyz / homoPosition.w;
    position = position * 0.5 + 0.5;
    float currentDepth = position.z;
    currentDepth = max(min(currentDepth, 1), 0);
    // If the fragment is outside the shadow frustum, we don't care about its depth.
    // Otherwise, the fragment's depth must be between [0, 1].

    float bias = max(0.05 * (1.0 - dot(normal, -uDirectionalShadow.dir)), 0.005);
    const float biasModifier = 0.5f;
    if (layer == NUM_CASCADES - 1) {
        bias *= 1 / (uDirectionalShadow.farPlaneDistance * biasModifier);
    } else {
        bias *= 1 / (uDirectionalShadow.cascadePlaneDistances[layer] * biasModifier);
    }

    float shadow = 0;
    vec2 texelSize = 1.0 / vec2(textureSize(uDirectionalShadow.shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(
                uDirectionalShadow.shadowMap, 
                vec3(position.xy + vec2(x, y) * texelSize, layer)
            ).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}
)";
}