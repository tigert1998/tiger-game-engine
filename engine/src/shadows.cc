#define NOMINMAX

// clang-format off
#include <glad/glad.h>
// clang-format on

#include "shadows.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include "utils.h"

DirectionalShadow::DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                                     uint32_t fbo_height, const Camera *camera)
    : fbo_width_(fbo_width), fbo_height_(fbo_height), camera_(camera) {
  std::vector<Texture> empty;
  Texture depth_texture(nullptr, GL_TEXTURE_2D_ARRAY, fbo_width, fbo_height,
                        NUM_CASCADES, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                        GL_FLOAT, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR,
                        {1, 1, 1, 1}, false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();

  previous.resize(NUM_CASCADES);
  set_direction(direction);
}

void DirectionalShadow::set_direction(glm::vec3 direction) {
  if (glm::distance(direction_, direction) > 1e-5) {
    for (int i = 0; i < NUM_CASCADES; i++) {
      // erase previous results
      previous[i].projection_matrix_ortho_param.min =
          glm::vec3(std::numeric_limits<float>::max());
      previous[i].projection_matrix_ortho_param.max =
          glm::vec3(std::numeric_limits<float>::lowest());
    }
  }

  direction_ = direction;
}

bool DirectionalShadow::ShouldUsePreviousResult(
    uint32_t index, const std::vector<glm::vec3> &frustum_corners) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  for (auto corner : frustum_corners) {
    auto corner_view_space = previous[index].view_matrix * glm::vec4(corner, 1);
    min_x = std::min(min_x, corner_view_space.x);
    max_x = std::max(max_x, corner_view_space.x);
    min_y = std::min(min_y, corner_view_space.y);
    max_y = std::max(max_y, corner_view_space.y);
    min_z = std::min(min_z, corner_view_space.z);
    max_z = std::max(max_z, corner_view_space.z);
  }

  glm::vec3 previous_min = previous[index].projection_matrix_ortho_param.min;
  glm::vec3 previous_max = previous[index].projection_matrix_ortho_param.max;
  return previous_min.x <= min_x && max_x <= previous_max.x &&
         previous_min.y <= min_y && max_y <= previous_max.y &&
         previous_min.z <= min_z && max_z <= previous_max.z;
}

void DirectionalShadow::CalcFrustumCorners(
    const std::function<void(bool, uint32_t, const std::vector<glm::vec3> &)>
        &callback) const {
  auto distances = cascade_plane_distances();

  for (int i = 0; i < NUM_CASCADES; i++) {
    auto corners =
        camera_->frustum_corners(distances[i * 2 + 0], distances[i * 2 + 1]);
    bool should_use_previous_result = ShouldUsePreviousResult(i, corners);

    callback(should_use_previous_result, i, corners);
  }
}

std::vector<OBB> DirectionalShadow::cascade_obbs() const {
  std::vector<OBB> obbs;
  obbs.reserve(NUM_CASCADES);

  CalcFrustumCorners([&](bool should_use_previous_result, uint32_t index,
                         const auto &corners) {
    obbs.push_back(cascade_obb(should_use_previous_result, index, corners));
  });

  return obbs;
}

std::vector<glm::mat4> DirectionalShadow::view_projection_matrices() const {
  std::vector<glm::mat4> matrices;
  matrices.reserve(NUM_CASCADES);

  CalcFrustumCorners([&](bool should_use_previous_result, uint32_t index,
                         const auto &corners) {
    if (should_use_previous_result) {
      matrices.push_back(previous[index].projection_matrix *
                         previous[index].view_matrix);
    } else {
      auto p = projection_matrix(corners);
      auto v = view_matrix(corners);
      auto param = projection_matrix_ortho_param(corners);
      matrices.push_back(p * v);
      previous[index].projection_matrix = p;
      previous[index].view_matrix = v;
      previous[index].projection_matrix_ortho_param = param;
    }
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

  // enlarge AABB
  auto extent = aabb.extents() * 1.5f;
  auto center = aabb.center();
  aabb.min = center - extent;
  aabb.max = center + extent;

  return aabb;
}

OBB DirectionalShadow::cascade_obb(
    bool should_use_previous_result, uint32_t index,
    const std::vector<glm::vec3> &frustum_corners) const {
  glm::mat4 v = should_use_previous_result ? previous[index].view_matrix
                                           : view_matrix(frustum_corners);
  AABB aabb = should_use_previous_result
                  ? previous[index].projection_matrix_ortho_param
                  : projection_matrix_ortho_param(frustum_corners);

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

OmnidirectionalShadow::OmnidirectionalShadow(glm::vec3 position, float radius,
                                             uint32_t fbo_width,
                                             uint32_t fbo_height)
    : position_(position),
      radius_(radius),
      fbo_width_(fbo_width),
      fbo_height_(fbo_height) {
  std::vector<Texture> empty;
  Texture depth_texture(std::vector<void *>{}, fbo_width, fbo_height,
                        GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT,
                        GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR, {1, 1, 1, 1},
                        false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();
}

std::vector<glm::mat4> OmnidirectionalShadow::view_projection_matrices() const {
  float aspect = (float)fbo_width_ / fbo_height_;
  glm::mat4 projection_matrix =
      glm::perspective(glm::radians(90.0f), aspect, z_near_, z_far_);

  std::vector<glm::mat4> ret;
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(1.0, 0.0, 0.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(-1.0, 0.0, 0.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 1.0, 0.0),
                            glm::vec3(0.0, 0.0, 1.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, -1.0, 0.0),
                            glm::vec3(0.0, 0.0, -1.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 0.0, 1.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 0.0, -1.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  return ret;
}

void OmnidirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_->Bind();
}

void OmnidirectionalShadow::Visualize() const {}

OmnidirectionalShadow::OmnidirectionalShadowGLSL
OmnidirectionalShadow::omnidirectional_shadow_glsl() const {
  OmnidirectionalShadowGLSL ret;
  auto vpms = view_projection_matrices();
  std::copy(vpms.begin(), vpms.end(), ret.view_projection_matrices);
  ret.shadow_map = fbo_->depth_texture().handle();
  ret.pos = position_;
  ret.radius = radius_;
  ret.far_plane = z_far_;
  return ret;
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