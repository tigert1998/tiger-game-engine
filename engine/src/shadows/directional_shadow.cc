#include "shadows/directional_shadow.h"

#include <glm/gtc/matrix_transform.hpp>

DirectionalShadow::DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                                     uint32_t fbo_height,
                                     std::optional<AABB> global_cascade_aabb,
                                     const Camera *camera)
    : fbo_width_(fbo_width),
      fbo_height_(fbo_height),
      global_cascade_aabb_(global_cascade_aabb),
      camera_(camera) {
  std::vector<Texture> empty;
  uint32_t num_cascades =
      enable_global_cascade() ? NUM_CASCADES : NUM_MOVING_CASCADES;
  Texture depth_texture(nullptr, GL_TEXTURE_2D_ARRAY, fbo_width, fbo_height,
                        num_cascades, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                        GL_FLOAT, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR,
                        {1, 1, 1, 1}, false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();

  set_direction(direction);
}

bool DirectionalShadow::enable_global_cascade() const {
  return global_cascade_aabb_.has_value();
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
  const float zero = 1e-5;
  if (glm::distance(direction_, direction) > zero) {
    InvalidatePreviousCascades();
  }

  direction_ = direction;
}

void DirectionalShadow::set_camera(Camera *camera) {
  InvalidatePreviousCascades();
  camera_ = camera;
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
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();

  glm::mat4 v = view_matrix(current_frame_frustum_corners);

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
  auto extent = aabb.extents() * 1.5f;
  extent.z = extent.z * 6;
  auto center = aabb.center();
  aabb.min = center - extent;
  aabb.max = center + extent;

  return aabb;
}

glm::mat4 DirectionalShadow::projection_matrix(
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  AABB aabb = ortho_param(current_frame_frustum_corners);
  return glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z,
                    aabb.max.z);
}

OBB DirectionalShadow::obb(
    const std::vector<glm::vec3> &current_frame_frustum_corners) const {
  glm::mat4 v = view_matrix(current_frame_frustum_corners);
  AABB aabb = ortho_param(current_frame_frustum_corners);

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

void DirectionalShadow::UpdateCascades() const {
  for (int i = 0; i < NUM_MOVING_CASCADES; i++) {
    auto [z_near, z_far] = cascade_plane_distances(i);
    cascades_[i].cascade_plane_distances[0] = z_near;
    cascades_[i].cascade_plane_distances[1] = z_far;
    auto corners =
        camera_->frustum_corners(cascades_[i].cascade_plane_distances[0],
                                 cascades_[i].cascade_plane_distances[1]);
    if (ShouldUsePreviousFrame(i, corners)) continue;

    cascades_[i].view_matrix = view_matrix(corners);
    cascades_[i].ortho_param = ortho_param(corners);
    cascades_[i].projection_matrix = projection_matrix(corners);
    cascades_[i].obb = obb(corners);
  }

  if (enable_global_cascade()) {
    uint32_t i = NUM_CASCADES - 1;
    auto corners = global_cascade_aabb_->corners();
    cascades_[i].view_matrix = view_matrix(corners);
    cascades_[i].ortho_param = ortho_param(corners);
    cascades_[i].projection_matrix = projection_matrix(corners);
    cascades_[i].obb = obb(corners);
  }
}

std::vector<OBB> DirectionalShadow::cascade_obbs() const {
  UpdateCascades();

  std::vector<OBB> obbs;
  obbs.reserve(NUM_CASCADES);
  for (int i = 0; i < NUM_MOVING_CASCADES; i++) {
    obbs.push_back(cascades_[i].obb);
  }
  if (enable_global_cascade()) {
    obbs.push_back(cascades_[NUM_CASCADES - 1].obb);
  }
  return obbs;
}

DirectionalShadow::DirectionalShadowGLSL
DirectionalShadow::directional_shadow_glsl() const {
  UpdateCascades();

  DirectionalShadowGLSL ret;
  ret.dir = direction_;
  ret.shadow_map = fbo_->depth_texture().handle();
  ret.has_global_cascade = enable_global_cascade();

  for (int i = 0; i < NUM_CASCADES; i++) {
    if (i < NUM_MOVING_CASCADES) {
      ret.cascade_plane_distances[i * 2 + 0] =
          cascades_[i].cascade_plane_distances[0];
      ret.cascade_plane_distances[i * 2 + 1] =
          cascades_[i].cascade_plane_distances[1];
    }
    ret.view_projection_matrices[i] =
        cascades_[i].projection_matrix * cascades_[i].view_matrix;
  }

  return ret;
}

void DirectionalShadow::Visualize() const {
  // TODO
}

void DirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_->Bind();
}

void DirectionalShadow::Unbind() { fbo_->Unbind(); }