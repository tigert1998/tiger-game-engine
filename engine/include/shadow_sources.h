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
#include "texture.h"

class Shadow {
 public:
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void Set(Shader *shader, int32_t *num_samplers) = 0;
  virtual void SetForDepthPass(Shader *shader) = 0;
  virtual void ImGuiWindow(uint32_t index,
                           const std::function<void()> &erase_callback) = 0;
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
  void Set(Shader *shader, int32_t *num_samplers) override;
  void SetForDepthPass(Shader *shader) override;
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
  static constexpr float CASCADE_PLANE_RATIO[NUM_CASCADES - 1] = {
      1 / 50.f,
      1 / 25.f,
      1 / 10.f,
      1 / 2.f,
  };

  inline std::vector<float> cascade_plane_distances() const {
    float dis = camera_->z_far() - camera_->z_near();
    std::vector<float> ret(NUM_CASCADES - 1);
    for (int i = 0; i < ret.size(); i++)
      ret[i] = camera_->z_near() + dis * CASCADE_PLANE_RATIO[i];
    return ret;
  }

  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;

  void Visualize() const override;
};

class ShadowSources {
 private:
  std::vector<std::unique_ptr<Shadow>> shadows_;
  const Camera *camera_;

 public:
  void Add(std::unique_ptr<Shadow> shadow);
  inline uint32_t Size() const { return shadows_.size(); }
  Shadow *Get(int32_t index);
  void Set(Shader *shader, int32_t *num_samplers);
  inline ShadowSources(const Camera *camera) : camera_(camera) {}

  void DrawDepthForShadow(const std::function<void(Shadow *)> &render_pass);
  void ImGuiWindow();
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