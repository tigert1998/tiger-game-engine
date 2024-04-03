#include "shadows/directional_shadow.h"

#include <fmt/core.h>

#include <glm/gtc/matrix_transform.hpp>

bool DirectionalShadow::enable_global_cascade() const {
  return global_cascade_aabb_.has_value();
}

uint32_t DirectionalShadow::num_cascades_in_use() const {
  return NUM_MOVING_CASCADES + (uint32_t)enable_global_cascade();
}

DirectionalShadow::DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                                     uint32_t fbo_height,
                                     std::optional<AABB> global_cascade_aabb,
                                     const Camera *camera)
    : fbo_width_(fbo_width),
      fbo_height_(fbo_height),
      global_cascade_aabb_(global_cascade_aabb),
      camera_(camera) {
  std::vector<Texture> empty;
  Texture depth_texture(nullptr, GL_TEXTURE_2D_ARRAY, fbo_width, fbo_height,
                        num_cascades_in_use(), GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_CLAMP_TO_EDGE,
                        GL_LINEAR, GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();

  direction_ = glm::vec3(0);
  set_direction(direction);

  glGetFloatv(GL_MAX_VIEWPORT_DIMS, &max_viewport_size_.x);
}

void DirectionalShadow::InvalidatePreviousCascades() {
  for (int i = 0; i < NUM_MOVING_CASCADES; i++) {
    // invalidate previous results
    cascades_[i].ortho_param.min = glm::vec3(std::numeric_limits<float>::max());
    cascades_[i].ortho_param.max =
        glm::vec3(std::numeric_limits<float>::lowest());
  }
}

void DirectionalShadow::set_direction(glm::vec3 direction) {
  const float zero = 1e-5f;
  if (glm::distance(direction_, direction) > zero) {
    InvalidatePreviousCascades();
    set_requires_update();
  }
  direction_ = direction;
}

void DirectionalShadow::set_camera(Camera *camera) {
  InvalidatePreviousCascades();
  camera_ = camera;

  set_requires_update();
}

std::pair<float, float> DirectionalShadow::cascade_plane_distances(
    uint32_t index) const {
  float dis = camera_->z_far() - camera_->z_near();
  float z_near = camera_->z_near() + dis * CASCADE_PLANE_RATIOS[index][0];
  float z_far = camera_->z_near() + dis * CASCADE_PLANE_RATIOS[index][1];
  return {z_near, z_far};
}

bool DirectionalShadow::ShouldUsePreviousFrame(
    uint32_t index,
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  for (auto corner : current_frame_frustum_corners) {
    auto corner_view_space =
        cascades_[index].view_matrix * glm::vec4(corner, 1);
    min_x = std::min(min_x, corner_view_space.x);
    max_x = std::max(max_x, corner_view_space.x);
    min_y = std::min(min_y, corner_view_space.y);
    max_y = std::max(max_y, corner_view_space.y);
    min_z = std::min(min_z, corner_view_space.z);
    max_z = std::max(max_z, corner_view_space.z);
  }

  glm::vec3 previous_min = cascades_[index].ortho_param.min;
  glm::vec3 previous_max = cascades_[index].ortho_param.max;
  return previous_min.x <= min_x && max_x <= previous_max.x &&
         previous_min.y <= min_y && max_y <= previous_max.y &&
         previous_min.z <= min_z && max_z <= previous_max.z;
}

glm::mat4 DirectionalShadow::view_matrix(
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  auto up = glm::vec3(0, 1, 0);
  if (std::abs(std::abs(glm::dot(up, glm::normalize(direction_))) - 1) <
      1e-8f) {
    up = glm::vec3(0, 0, 1);
  }
  glm::vec3 center = glm::vec3(0);
  for (auto corner : current_frame_frustum_corners) {
    center += corner;
  }
  center /= current_frame_frustum_corners.size();
  return glm::lookAt(center - direction_, center, up);
}

AABB DirectionalShadow::ortho_param(
    const std::vector<glm::vec3> &current_frame_frustum_corners,
    glm::mat4 view_matrix) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  glm::mat4 v = view_matrix;

  for (auto corner : current_frame_frustum_corners) {
    auto corner_view_space = v * glm::vec4(corner, 1);
    min_x = std::min(min_x, corner_view_space.x);
    max_x = std::max(max_x, corner_view_space.x);
    min_y = std::min(min_y, corner_view_space.y);
    max_y = std::max(max_y, corner_view_space.y);
    min_z = std::min(min_z, corner_view_space.z);
    max_z = std::max(max_z, corner_view_space.z);
  }

  AABB aabb;
  aabb.min = glm::vec3(min_x, min_y, min_z);
  aabb.max = glm::vec3(max_x, max_y, max_z);

  // enlarge AABB
  auto extent = aabb.extents() * ENLARGE_FACTOR;
  auto center = aabb.center();
  aabb.min = center - extent;
  aabb.max = center + extent;

  return aabb;
}

glm::mat4 DirectionalShadow::projection_matrix(AABB ortho_param) const {
  AABB aabb = ortho_param;
  return glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z,
                    aabb.max.z);
}

OBB DirectionalShadow::obb(
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  glm::mat4 v = view_matrix(current_frame_frustum_corners);
  AABB aabb = ortho_param(current_frame_frustum_corners, v);

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

void DirectionalShadow::set_requires_update() { update_flag_ = true; }

void DirectionalShadow::UpdateCascades() const {
  auto camera_frustum_corners =
      camera_->frustum_corners(camera_->z_near(), camera_->z_far());
  glm::mat4 camera_frustum_view_matrix =
      this->view_matrix(camera_frustum_corners);
  AABB camera_ortho_param =
      ortho_param(camera_frustum_corners, camera_frustum_view_matrix);

  for (int i = 0; i < num_cascades_in_use(); i++) {
    std::vector<glm::vec3> corners;

    if (i < NUM_MOVING_CASCADES) {
      auto [z_near, z_far] = cascade_plane_distances(i);
      corners = camera_->frustum_corners(z_near, z_far);
      bool should_use_previous_frame = ShouldUsePreviousFrame(i, corners);
      cascades_[i].requires_update = update_flag_ || !should_use_previous_frame;
      if (!cascades_[i].requires_update) continue;
      cascades_[i].cascade_plane_distances[0] = z_near;
      cascades_[i].cascade_plane_distances[1] = z_far;
    } else {
      corners = global_cascade_aabb_->corners();
      cascades_[i].requires_update = update_flag_;
      if (!cascades_[i].requires_update) continue;
    }

    cascades_[i].view_matrix = camera_frustum_view_matrix;
    AABB ortho_param = this->ortho_param(corners, camera_frustum_view_matrix);
    cascades_[i].ortho_param =
        AABB(glm::vec3(glm::vec2(ortho_param.min), camera_ortho_param.min.z),
             glm::vec3(glm::vec2(ortho_param.max), camera_ortho_param.max.z));
    cascades_[i].projection_matrix =
        projection_matrix(cascades_[i].ortho_param);
    cascades_[i].obb = obb(corners);
    auto [viewport, scale_factor] =
        this->viewport(corners, camera_ortho_param, camera_frustum_view_matrix);
    cascades_[i].viewport = viewport;
    cascades_[i].scale_factor = scale_factor;
  }
}

std::pair<glm::vec4, glm::vec2> DirectionalShadow::viewport(
    const std::vector<glm::vec3> &current_frame_frustum_corners,
    const AABB &camera_ortho_param,
    glm::mat4 camera_frustum_view_matrix) const {
  AABB ortho_param = this->ortho_param(current_frame_frustum_corners,
                                       camera_frustum_view_matrix);
  glm::vec2 cascade_size = glm::vec2(ortho_param.max.x - ortho_param.min.x,
                                     ortho_param.max.y - ortho_param.min.y);
  glm::vec2 camera_size =
      glm::vec2(camera_ortho_param.max.x - camera_ortho_param.min.x,
                camera_ortho_param.max.y - camera_ortho_param.min.y);
  glm::vec2 ratio = glm::vec2(fbo_width_, fbo_height_) / cascade_size;
  glm::vec2 camera_size_pixels = camera_size * ratio;
  glm::vec2 cascade_offset =
      glm::vec2(ortho_param.min.x - camera_ortho_param.min.x,
                ortho_param.min.y - camera_ortho_param.min.y);
  glm::vec2 cascade_offset_pixels = cascade_offset * ratio;

  glm::vec2 scale_factor =
      glm::min(glm::vec2(1), max_viewport_size_ / camera_size_pixels);

  glm::vec4 viewport = glm::vec4(-cascade_offset_pixels * scale_factor,
                                 camera_size_pixels * scale_factor);
  return {viewport, scale_factor};
}

std::vector<OBB> DirectionalShadow::cascade_obbs() const {
  std::vector<OBB> obbs;
  obbs.reserve(num_cascades_in_use());
  for (int i = 0; i < num_cascades_in_use(); i++) {
    obbs.push_back(cascades_[i].obb);
  }
  return obbs;
}

DirectionalShadow::DirectionalShadowGLSL
DirectionalShadow::directional_shadow_glsl() const {
  DirectionalShadowGLSL ret;
  ret.dir = direction_;
  ret.shadow_map = fbo_->depth_texture().handle();

  for (int i = 0; i < num_cascades_in_use(); i++) {
    if (i < NUM_MOVING_CASCADES) {
      ret.cascade_plane_distances[i * 2 + 0] =
          cascades_[i].cascade_plane_distances[0];
      ret.cascade_plane_distances[i * 2 + 1] =
          cascades_[i].cascade_plane_distances[1];
    }
    ret.transformation_matrices[i] =
        glm::translate(glm::mat4(1),
                       glm::vec3(0.5f * cascades_[i].scale_factor, 0.5f)) *
        glm::scale(glm::mat4(1),
                   glm::vec3(0.5f * cascades_[i].scale_factor, 0.5f)) *
        cascades_[i].projection_matrix * cascades_[i].view_matrix;
    ret.requires_update[i] = cascades_[i].requires_update;
  }

  return ret;
}

void DirectionalShadow::Visualize() const {
  // TODO
}

void DirectionalShadow::Bind() {
  fbo_->Bind();
  UpdateCascades();
  for (int i = 0; i < NUM_CASCADES; i++) {
    glViewportIndexedfv(i, &cascades_[i].viewport.x);
  }
}

void DirectionalShadow::Unbind() {
  fbo_->Unbind();
  update_flag_ = false;
}

void DirectionalShadow::Clear() {
  const float one = 1;
  for (int i = 0; i < num_cascades_in_use(); i++) {
    if (cascades_[i].requires_update) {
      glClearTexSubImage(fbo_->depth_texture().id(), 0, 0, 0, i, fbo_width_,
                         fbo_height_, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &one);
    }
  }
}

void DirectionalShadow::Set(Shader *shader) {
  auto camera_frustum_corners =
      camera_->frustum_corners(camera_->z_near(), camera_->z_far());
  glm::mat4 camera_frustum_view_matrix =
      this->view_matrix(camera_frustum_corners);
  AABB camera_ortho_param =
      ortho_param(camera_frustum_corners, camera_frustum_view_matrix);
  glm::mat4 camera_frustum_projection_matrix =
      this->projection_matrix(camera_ortho_param);

  shader->SetUniform<glm::mat4>("uViewMatrix", camera_frustum_view_matrix);
  shader->SetUniform<glm::mat4>("uProjectionMatrix",
                                camera_frustum_projection_matrix);
}