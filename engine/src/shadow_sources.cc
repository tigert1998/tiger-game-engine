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
  std::vector<Texture> empty;
  Texture depth_texture(nullptr, GL_TEXTURE_2D_ARRAY, fbo_width, fbo_height,
                        NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                        GL_FLOAT, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR,
                        {1, 1, 1, 1}, false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
}

void DirectionalShadow::CalcFrustumCorners(
    const std::function<void(const std::vector<glm::vec3> &)> &callback) const {
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

    callback(corners);
  }
}

std::vector<OBB> DirectionalShadow::cascade_obbs() const {
  std::vector<OBB> obbs;
  obbs.reserve(NUM_CASCADES);

  CalcFrustumCorners(
      [&](const auto &corners) { obbs.push_back(cascade_obb(corners)); });

  return obbs;
}

std::vector<glm::mat4> DirectionalShadow::view_projection_matrices() const {
  std::vector<glm::mat4> matrices;
  matrices.reserve(NUM_CASCADES);

  CalcFrustumCorners([&](const auto &corners) {
    matrices.push_back(projection_matrix(corners) * view_matrix(corners));
  });

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

AABB DirectionalShadow::projection_matrix_ortho_param(
    const std::vector<glm::vec3> &frustum_corners) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  glm::mat4 v = view_matrix(frustum_corners);

  for (auto corner : frustum_corners) {
    auto corner_view_space = v * glm::vec4(corner, 1);
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

  AABB aabb;
  aabb.min = glm::vec3(min_x, min_y, min_z);
  aabb.max = glm::vec3(max_x, max_y, max_z);
  return aabb;
}

OBB DirectionalShadow::cascade_obb(
    const std::vector<glm::vec3> &frustum_corners) const {
  glm::mat4 v = view_matrix(frustum_corners);
  AABB aabb = projection_matrix_ortho_param(frustum_corners);

  glm::mat4 translation = glm::mat4(1);
  translation[3].x = v[3].x;
  translation[3].y = v[3].y;
  translation[3].z = v[3].z;

  glm::mat4 scaling =
      glm::scale(glm::mat4(1), glm::vec3(glm::length(glm::vec3(v[0])),
                                         glm::length(glm::vec3(v[1])),
                                         glm::length(glm::vec3(v[2]))));

  glm::mat4 rotation = glm::mat4(1);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) {
      rotation[i][j] = v[i][j] / scaling[i][i];
    }

  OBB obb;
  glm::mat4 transform = glm::inverse(translation * scaling);
  obb.min = transform * glm::vec4(aabb.min, 1);
  obb.max = transform * glm::vec4(aabb.max, 1);
  obb.set_rotation(glm::inverse(rotation));
  return obb;
}

glm::mat4 DirectionalShadow::projection_matrix(
    const std::vector<glm::vec3> &frustum_corners) const {
  AABB aabb = projection_matrix_ortho_param(frustum_corners);
  return glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z,
                    aabb.max.z);
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

  ImGui::InputInt(
      fmt::format("Visualize layer##shadow_source_{}", index).c_str(),
      &imgui_visualize_layer_);
  ImGui::InputFloat4(
      fmt::format("Visualize viewport##shadow_source_{}", index).c_str(),
      &imgui_visualize_viewport.x);
  if (0 <= imgui_visualize_layer_ && imgui_visualize_layer_ < NUM_CASCADES) {
    if (kViewer == nullptr) kViewer.reset(new DirectionalShadowViewer());
  }
}

void DirectionalShadow::Visualize() const {
  if (0 <= imgui_visualize_layer_ && imgui_visualize_layer_ < NUM_CASCADES) {
    kViewer->Draw(imgui_visualize_viewport, fbo_->depth_texture(),
                  imgui_visualize_layer_);
  }
}

std::unique_ptr<DirectionalShadowViewer> DirectionalShadow::kViewer = nullptr;

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

void ShadowSources::Visualize() {
  for (const auto &shadow : shadows_) {
    shadow->Visualize();
  }
}

std::unique_ptr<Shader> DirectionalShadowViewer::kShader = nullptr;

DirectionalShadowViewer::DirectionalShadowViewer() {
  glGenVertexArrays(1, &vao_);
  if (kShader == nullptr) {
    kShader =
        Shader::ScreenSpaceShader("shadow/directional_shadow_viewer.frag", {});
  }
}

DirectionalShadowViewer::~DirectionalShadowViewer() {
  glDeleteVertexArrays(1, &vao_);
}

void DirectionalShadowViewer::Draw(glm::vec4 viewport,
                                   const Texture &shadow_map, uint32_t layer) {
  glBindVertexArray(vao_);

  kShader->Use();
  kShader->SetUniformSampler("uDirectionalShadowMap", shadow_map, 0);
  kShader->SetUniform<glm::vec4>("uViewport", viewport);
  kShader->SetUniform<uint32_t>("uLayer", layer);

  glViewport(viewport.x, viewport.y, viewport.z, viewport.w);
  glDrawArrays(GL_POINTS, 0, 1);

  glBindVertexArray(0);
}