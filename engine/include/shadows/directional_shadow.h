#ifndef SHADOWS_DIRECTIONAL_SHADOW_H_
#define SHADOWS_DIRECTIONAL_SHADOW_H_

#include <glm/glm.hpp>

#include "aabb.h"
#include "obb.h"
#include "shadows/shadow.h"

class DirectionalShadow : public Shadow {
 public:
  static constexpr uint32_t NUM_MOVING_CASCADES = 5;
  static constexpr uint32_t NUM_CASCADES = NUM_MOVING_CASCADES + 1;
  static constexpr float CASCADE_PLANE_RATIOS[NUM_MOVING_CASCADES][2] = {
      {0.00f, 0.02f}, {0.02f, 0.05f}, {0.05f, 0.1f}, {0.1f, 0.5f}, {0.5f, 1.0f},
  };

 private:
  glm::vec3 direction_;
  uint32_t fbo_width_, fbo_height_;
  std::optional<AABB> global_cascade_aabb_;
  std::unique_ptr<FrameBufferObject> fbo_;
  const Camera *camera_;

  struct Cascade {
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
    float cascade_plane_distances[2];
    AABB ortho_param;
    OBB obb;
  };

  mutable Cascade cascades_[NUM_CASCADES];

  std::pair<float, float> cascade_plane_distances(uint32_t index) const;
  bool ShouldUsePreviousFrame(
      uint32_t index,
      const std::vector<glm::vec3> &current_frame_frustum_corners) const;
  glm::mat4 view_matrix(
      const std::vector<glm::vec3> &current_frame_frustum_corners) const;
  AABB ortho_param(
      const std::vector<glm::vec3> &current_frame_frustum_corners) const;
  glm::mat4 projection_matrix(
      const std::vector<glm::vec3> &current_frame_frustum_corners) const;
  OBB obb(const std::vector<glm::vec3> &current_frame_frustum_corners) const;
  void InvalidatePreviousCascades();

  void UpdateCascades() const;

 public:
  explicit DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                             uint32_t fbo_height,
                             std::optional<AABB> global_cascade_aabb,
                             const Camera *camera);

  bool enable_global_cascade() const;

  inline glm::vec3 direction() { return direction_; }
  void set_direction(glm::vec3 direction);
  void set_camera(Camera *camera);

  std::vector<OBB> cascade_obbs() const;

  struct DirectionalShadowGLSL {
    alignas(16) glm::mat4 view_projection_matrices[NUM_CASCADES];
    float cascade_plane_distances[NUM_MOVING_CASCADES * 2];
    uint64_t shadow_map;
    glm::vec3 dir;
  };

  DirectionalShadowGLSL directional_shadow_glsl() const;

  void Visualize() const override;
  void Bind() override;
  void Unbind() override;
};

#endif