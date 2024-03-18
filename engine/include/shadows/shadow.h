#ifndef SHADOWS_SHADOW_H_
#define SHADOWS_SHADOW_H_

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