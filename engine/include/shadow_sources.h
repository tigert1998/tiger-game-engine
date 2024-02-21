#ifndef SHADOW_SOURCES_H_
#define SHADOW_SOURCES_H_

#include <functional>
#include <glm/glm.hpp>
#include <memory>

#include "aabb.h"
#include "camera.h"
#include "frame_buffer_object.h"
#include "obb.h"
#include "shader.h"
#include "ssbo.h"
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
  std::unique_ptr<FrameBufferObject> fbo_;
  const Camera *camera_;

  int32_t imgui_visualize_layer_ = -1;
  glm::vec4 imgui_visualize_viewport = glm::vec4(0, 0, 128, 128);

  AABB projection_matrix_ortho_param(
      const std::vector<glm::vec3> &frustum_corners) const;
  void CalcFrustumCorners(
      const std::function<void(const std::vector<glm::vec3> &)> &callback)
      const;

  static std::unique_ptr<DirectionalShadowViewer> kViewer;

 public:
  DirectionalShadow(glm::vec3 direction, uint32_t fbo_width,
                    uint32_t fbo_height, const Camera *camera);
  void Bind() override;
  inline void Unbind() override { fbo_->Unbind(); }
  inline ~DirectionalShadow() override {}

  inline glm::vec3 direction() { return direction_; }
  inline void set_direction(glm::vec3 direction) { direction_ = direction; }

  std::vector<OBB> cascade_obbs() const;
  OBB cascade_obb(const std::vector<glm::vec3> &frustum_corners) const;

  std::vector<glm::mat4> view_projection_matrices() const;
  glm::mat4 view_matrix(const std::vector<glm::vec3> &frustum_corners) const;
  glm::mat4 projection_matrix(
      const std::vector<glm::vec3> &frustum_corners) const;

  static constexpr uint32_t NUM_CASCADES = 5;
  static constexpr uint32_t GLSL_BINDING = 16;

  static constexpr float CASCADE_PLANE_RATIO[NUM_CASCADES][2] = {
      {0, 0.03}, {0.02, 0.04}, {0.03, 0.1}, {0.07, 0.5}, {0.4, 1},
  };

  inline std::vector<float> cascade_plane_distances() const {
    float dis = camera_->z_far() - camera_->z_near();
    std::vector<float> ret(NUM_CASCADES * 2);
    for (int i = 0; i < NUM_CASCADES; i++)
      for (int j = 0; j < 2; j++)
        ret[i * 2 + j] = camera_->z_near() + dis * CASCADE_PLANE_RATIO[i][j];
    return ret;
  }

  void Visualize() const override;

  struct DirectionalShadowGLSL {
    glm::mat4 view_projection_matrices[NUM_CASCADES];
    float cascade_plane_distances[NUM_CASCADES * 2];
    uint64_t shadow_map;
    glm::vec3 dir;
  };

  DirectionalShadowGLSL directional_shadow_glsl() const;
};

class OmnidirectionalShadow : public Shadow {
 private:
  glm::vec3 position_;
  uint32_t fbo_width_, fbo_height_;
  std::unique_ptr<FrameBufferObject> fbo_;

 public:
  OmnidirectionalShadow(glm::vec3 position, uint32_t fbo_width,
                        uint32_t fbo_height);
  void Bind() override;
  inline void Unbind() override { fbo_->Unbind(); }
  inline glm::vec3 position() { return position_; }
  inline void set_position(glm::vec3 position) { position_ = position; }
  inline ~OmnidirectionalShadow() override {}

  void Visualize() const override;

  static constexpr uint32_t GLSL_BINDING = 17;

  struct OmnidirectionalShadowGLSL {
    uint64_t shadow_map;
    glm::vec3 pos;
  };

  OmnidirectionalShadowGLSL omnidirectional_shadow_glsl() const;
};

class ShadowSources {
 private:
  std::unique_ptr<SSBO> directional_shadows_ssbo_;
  std::unique_ptr<SSBO> omnidirectional_shadows_ssbo_;
  std::vector<std::unique_ptr<DirectionalShadow>> directional_shadows_;
  std::vector<std::unique_ptr<OmnidirectionalShadow>> omnidirectional_shadows_;

 public:
  explicit ShadowSources();
  void AddDirectional(std::unique_ptr<DirectionalShadow> shadow);
  void AddOmnidirectional(std::unique_ptr<OmnidirectionalShadow> shadow);
  void EraseDirectional(DirectionalShadow *shadow);
  void EraseOmnidirectional(OmnidirectionalShadow *shadow);
  int32_t GetDirectionalIndex(DirectionalShadow *shadow);
  int32_t GetOmnidirectionalIndex(OmnidirectionalShadow *shadow);

  inline uint32_t SizeDirectional() const {
    return directional_shadows_.size();
  }
  inline uint32_t SizeOmnidirectional() const {
    return omnidirectional_shadows_.size();
  }
  DirectionalShadow *GetDirectional(int32_t index);
  OmnidirectionalShadow *GetOmnidirectional(int32_t index);
  void Set(Shader *shader);
  void DrawDepthForShadow(
      const std::function<void(int32_t, int32_t)> &render_pass);
  void Visualize();
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