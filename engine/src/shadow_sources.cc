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
  fbo_->depth_texture().MakeResident();
}

void DirectionalShadow::CalcFrustumCorners(
    const std::function<void(const std::vector<glm::vec3> &)> &callback) const {
  auto distances = cascade_plane_distances();

  for (int i = 0; i < NUM_CASCADES; i++) {
    auto corners =
        camera_->frustum_corners(distances[i * 2 + 0], distances[i * 2 + 1]);

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

DirectionalShadow::DirectionalShadowGLSL
DirectionalShadow::directional_shadow_glsl() const {
  DirectionalShadowGLSL ret;
  auto vpms = view_projection_matrices();
  std::copy(vpms.begin(), vpms.end(), ret.view_projection_matrices);
  auto cpds = cascade_plane_distances();
  std::copy(cpds.begin(), cpds.end(), ret.cascade_plane_distances);
  ret.shadow_map = fbo_->depth_texture().handle();
  ret.dir = direction_;

  return ret;
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

void DirectionalShadow::Visualize() const {
  if (0 <= imgui_visualize_layer_ && imgui_visualize_layer_ < NUM_CASCADES) {
    kViewer->Draw(imgui_visualize_viewport, fbo_->depth_texture(),
                  imgui_visualize_layer_);
  }
}

std::unique_ptr<DirectionalShadowViewer> DirectionalShadow::kViewer = nullptr;

OmnidirectionalShadow::OmnidirectionalShadow(glm::vec3 position,
                                             uint32_t fbo_width,
                                             uint32_t fbo_height)
    : position_(position), fbo_width_(fbo_width), fbo_height_(fbo_height) {
  std::vector<Texture> empty;
  Texture depth_texture(std::vector<void *>{}, fbo_width, fbo_height,
                        GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT,
                        GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR, {1, 1, 1, 1},
                        false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();
}

void OmnidirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_->Bind();
}

void OmnidirectionalShadow::Visualize() const {}

OmnidirectionalShadow::OmnidirectionalShadowGLSL
OmnidirectionalShadow::omnidirectional_shadow_glsl() const {
  OmnidirectionalShadowGLSL ret;
  ret.shadow_map = fbo_->depth_texture().handle();
  ret.pos = position_;
  return ret;
}

ShadowSources::ShadowSources() {
  directional_shadows_ssbo_.reset(
      new SSBO(0, nullptr, GL_DYNAMIC_DRAW, DirectionalShadow::GLSL_BINDING));
  omnidirectional_shadows_ssbo_.reset(new SSBO(
      0, nullptr, GL_DYNAMIC_DRAW, OmnidirectionalShadow::GLSL_BINDING));
}

void ShadowSources::AddDirectional(std::unique_ptr<DirectionalShadow> shadow) {
  directional_shadows_.emplace_back(std::move(shadow));
  directional_shadows_ssbo_.reset(
      new SSBO(sizeof(DirectionalShadow::DirectionalShadowGLSL) *
                   directional_shadows_.size(),
               nullptr, GL_DYNAMIC_DRAW, DirectionalShadow::GLSL_BINDING));
}

void ShadowSources::AddOmnidirectional(
    std::unique_ptr<OmnidirectionalShadow> shadow) {
  omnidirectional_shadows_.emplace_back(std::move(shadow));
  omnidirectional_shadows_ssbo_.reset(
      new SSBO(sizeof(OmnidirectionalShadow::OmnidirectionalShadowGLSL) *
                   omnidirectional_shadows_.size(),
               nullptr, GL_DYNAMIC_DRAW, OmnidirectionalShadow::GLSL_BINDING));
}

void ShadowSources::EraseDirectional(DirectionalShadow *shadow) {
  int32_t index = GetDirectionalIndex(shadow);
  if (index >= 0) {
    directional_shadows_.erase(directional_shadows_.begin() + index);
  }
}

void ShadowSources::EraseOmnidirectional(OmnidirectionalShadow *shadow) {
  int32_t index = GetOmnidirectionalIndex(shadow);
  if (index >= 0) {
    omnidirectional_shadows_.erase(omnidirectional_shadows_.begin() + index);
  }
}

int32_t ShadowSources::GetDirectionalIndex(DirectionalShadow *shadow) {
  for (int i = 0; i < directional_shadows_.size(); i++) {
    if (directional_shadows_[i].get() == shadow) return i;
  }
  return -1;
}

int32_t ShadowSources::GetOmnidirectionalIndex(OmnidirectionalShadow *shadow) {
  for (int i = 0; i < omnidirectional_shadows_.size(); i++) {
    if (omnidirectional_shadows_[i].get() == shadow) return i;
  }
  return -1;
}

DirectionalShadow *ShadowSources::GetDirectional(int32_t index) {
  return directional_shadows_[index].get();
}

OmnidirectionalShadow *ShadowSources::GetOmnidirectional(int32_t index) {
  return omnidirectional_shadows_[index].get();
}

void ShadowSources::Set(Shader *shader) {
  // directional
  std::vector<DirectionalShadow::DirectionalShadowGLSL> dir_shadow_glsl_vec;
  for (const auto &shadow : directional_shadows_) {
    dir_shadow_glsl_vec.push_back(shadow->directional_shadow_glsl());
  }
  glNamedBufferSubData(
      directional_shadows_ssbo_->id(), 0,
      dir_shadow_glsl_vec.size() * sizeof(dir_shadow_glsl_vec[0]),
      dir_shadow_glsl_vec.data());
  directional_shadows_ssbo_->BindBufferBase();

  if (shader->UniformVariableExists("uNumDirectionalShadows")) {
    shader->SetUniform<uint32_t>("uNumDirectionalShadows",
                                 directional_shadows_.size());
  }

  // omnidirectional
  std::vector<OmnidirectionalShadow::OmnidirectionalShadowGLSL>
      omnidir_shadow_glsl_vec;
  for (const auto &shadow : omnidirectional_shadows_) {
    omnidir_shadow_glsl_vec.push_back(shadow->omnidirectional_shadow_glsl());
  }
  glNamedBufferSubData(
      omnidirectional_shadows_ssbo_->id(), 0,
      omnidir_shadow_glsl_vec.size() * sizeof(omnidir_shadow_glsl_vec[0]),
      omnidir_shadow_glsl_vec.data());
  omnidirectional_shadows_ssbo_->BindBufferBase();

  if (shader->UniformVariableExists("uNumOmnidirectionalShadows")) {
    shader->SetUniform<uint32_t>("uNumOmnidirectionalShadows",
                                 omnidirectional_shadows_.size());
  }
}

void ShadowSources::DrawDepthForShadow(
    const std::function<void(int32_t, int32_t)> &render_pass) {
  glDisable(GL_CULL_FACE);
  for (int i = 0; i < directional_shadows_.size(); i++) {
    directional_shadows_[i]->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    render_pass(i, -1);
    directional_shadows_[i]->Unbind();
  }
  for (int i = 0; i < omnidirectional_shadows_.size(); i++) {
    omnidirectional_shadows_[i]->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    render_pass(-1, i);
    omnidirectional_shadows_[i]->Unbind();
  }
}

void ShadowSources::Visualize() {
  for (const auto &shadow : directional_shadows_) {
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