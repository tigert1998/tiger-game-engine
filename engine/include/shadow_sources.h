#ifndef SHADOW_SOURCES_H_
#define SHADOW_SOURCES_H_

#include <functional>
#include <glm/glm.hpp>

#include "frame_buffer_object.h"
#include "shader.h"

class Shadow {
 public:
  virtual void Bind() = 0;
  virtual void Unbind() = 0;
  virtual void Set(Shader *shader, int32_t *num_samplers) = 0;
  virtual void SetForDepthPass(Shader *shader) = 0;
  virtual ~Shadow(){};
};

class DirectionalShadow : public Shadow {
 private:
  glm::vec3 position_, direction_;
  float width_, height_, near_, far_;
  uint32_t fbo_width_, fbo_height_;
  FrameBufferObject fbo_;

 public:
  DirectionalShadow(glm::vec3 position, glm::vec3 direction, float width,
                    float height, float z_near, float z_far, uint32_t fbo_width,
                    uint32_t fbo_height);
  void Bind() override;
  inline void Unbind() override { fbo_.Unbind(); }
  void Set(Shader *shader, int32_t *num_samplers) override;
  void SetForDepthPass(Shader *shader) override;
  inline ~DirectionalShadow() override {}

  inline glm::vec3 position() { return position_; }
  inline void set_position(glm::vec3 position) { position_ = position; }
  inline glm::vec3 direction() { return direction_; }
  inline void set_direction(glm::vec3 direction) { direction_ = direction; }

  glm::mat4 view_matrix() const;
  glm::mat4 projection_matrix() const;
};

class ShadowSources : public Shadow {
 private:
  std::vector<std::unique_ptr<Shadow>> shadows_;

 public:
  void Add(std::unique_ptr<Shadow> shadow);
  Shadow *Get(int32_t index);
  inline void Bind() override {}
  inline void Unbind() override {}
  void Set(Shader *shader, int32_t *num_samplers) override;
  inline void SetForDepthPass(Shader *shader) override {}
  static std::string FsSource();
  inline ~ShadowSources() override {}

  void DrawDepthForShadow(const std::function<void(Shadow *)> &render_pass);
};

#endif