#ifndef SHADOWS_H_
#define SHADOWS_H_

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>

#include "aabb.h"
#include "camera.h"
#include "frame_buffer_object.h"
#include "obb.h"
#include "ogl_buffer.h"
#include "shader.h"
#include "texture.h"

class Shadow {
 public:
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void Visualize() const = 0;
  virtual ~Shadow(){};
};

class DirectionalShadowViewer;

class DirectionalShadow : public Shadow {
 private:
  glm::vec3 direction_;
  uint32_t fbo_width_, fbo_height_;
  std::optional<AABB> global_cascade_aabb_;
  std::unique_ptr<FrameBufferObject> fbo_;
  const Camera *camera_;

  int32_t imgui_visualize_layer_ = -1;
  glm::vec4 imgui_visualize_viewport_ = glm::vec4(0, 0, 128, 128);

  AABB projection_matrix_ortho_param(
      const std::vector<glm::vec3> &frustum_corners) const;

  // if use global cascade, returns NUM_CASCADES cascades
  // else returns (NUM_CASCADES - 1) cascades
  void CalcFrustumCorners(
      const std::function<void(bool, uint32_t, const std::vector<glm::vec3> &)>
          &callback) const;

  static std::unique_ptr<DirectionalShadowViewer> kViewer;

  struct PreviousBoundingBoxAndMatrices {
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
    AABB projection_matrix_ortho_param;
  };
  mutable std::vector<PreviousBoundingBoxAndMatrices> previous;

  bool ShouldUsePreviousResult(
      uint32_t index, const std::vector<glm::vec3> &frustum_corners) const;

 public:
  DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                    uint32_t fbo_height,
                    std::optional<AABB> global_cascade_aabb,
                    const Camera *camera);
  void Bind() override;
  inline void Unbind() override { fbo_->Unbind(); }
  inline ~DirectionalShadow() override {}

  inline bool enable_global_cascade() const {
    return global_cascade_aabb_.has_value();
  }

  inline glm::vec3 direction() { return direction_; }
  void set_direction(glm::vec3 direction);

  // if use global cascade, returns NUM_CASCADES cascades
  // else returns (NUM_CASCADES - 1) cascades
  std::vector<OBB> cascade_obbs() const;
  OBB cascade_obb(bool should_use_previous_result, uint32_t index,
                  const std::vector<glm::vec3> &frustum_corners) const;

  // if use global cascade, returns NUM_CASCADES cascades
  // else returns (NUM_CASCADES - 1) cascades
  std::vector<glm::mat4> view_projection_matrices() const;
  glm::mat4 view_matrix(const std::vector<glm::vec3> &frustum_corners) const;
  glm::mat4 projection_matrix(
      const std::vector<glm::vec3> &frustum_corners) const;

  static constexpr uint32_t NUM_CASCADES = 6;

  static constexpr float CASCADE_PLANE_RATIO[NUM_CASCADES - 1][2] = {
      {0.00f, 0.02f}, {0.02f, 0.05f}, {0.05f, 0.1f}, {0.1f, 0.5f}, {0.5f, 1.0f},
  };

  // returns NUM_CASCADES cascades
  std::vector<float> cascade_plane_distances() const;

  void Visualize() const override;

  struct DirectionalShadowGLSL {
    alignas(16) glm::mat4 view_projection_matrices[NUM_CASCADES];
    float cascade_plane_distances[NUM_CASCADES * 2];
    uint64_t shadow_map;
    glm::vec3 dir;
  };

  DirectionalShadowGLSL directional_shadow_glsl() const;
};

class OmnidirectionalShadow : public Shadow {
 private:
  glm::vec3 position_;
  float radius_;
  uint32_t fbo_width_, fbo_height_;
  std::unique_ptr<FrameBufferObject> fbo_;
  float z_near_ = 1e-2, z_far_ = 1e2;

 public:
  OmnidirectionalShadow(glm::vec3 position, float radius, uint32_t fbo_width,
                        uint32_t fbo_height);
  void Bind() override;
  inline void Unbind() override { fbo_->Unbind(); }
  inline glm::vec3 position() { return position_; }
  inline void set_position(glm::vec3 position) { position_ = position; }
  inline float radius() { return radius_; }
  inline void set_radius(float radius) { radius_ = radius; }
  std::vector<glm::mat4> view_projection_matrices() const;
  inline ~OmnidirectionalShadow() override {}

  void Visualize() const override;

  struct OmnidirectionalShadowGLSL {
    glm::mat4 view_projection_matrices[6];
    uint64_t shadow_map;
    alignas(16) glm::vec3 pos;
    float radius;
    float far_plane;
  };

  OmnidirectionalShadowGLSL omnidirectional_shadow_glsl() const;
};

class DirectionalShadowViewer {
 private:
  uint32_t vao_;
  static std::unique_ptr<Shader> kShader;

 public:
  explicit DirectionalShadowViewer();
  ~DirectionalShadowViewer();

  void Draw(glm::vec4 viewport, const Texture &shadow_map, uint32_t layer);
};

#endif