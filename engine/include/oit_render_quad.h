#ifndef OIT_RENDER_QUAD_H_
#define OIT_RENDER_QUAD_H_

#include <glad/glad.h>
#include <stdint.h>

#include <memory>

#include "frame_buffer_object.h"
#include "shader.h"

class OITRenderQuad {
 private:
  uint32_t width_, height_;
  uint32_t head_pointer_texture_, head_pointer_initializer_,
      atomic_counter_buffer_, fragment_storage_buffer_,
      fragment_storage_texture_;
  std::unique_ptr<FrameBufferObject> fbo_;

  void Deallocate();
  void Allocate(uint32_t width, uint32_t height, float fragment_per_pixel);

  const static std::string kFsSource;

  static std::shared_ptr<Shader> kShader;
  static uint32_t vao_;

  std::shared_ptr<Shader> shader_;

 public:
  OITRenderQuad(uint32_t width, uint32_t height);
  void Resize(uint32_t width, uint32_t height);

  inline void BindFrameBuffer() { fbo_->Bind(); }
  inline void UnbindFrameBuffer() { fbo_->Unbind(); }
  void CopyDepthToDefaultFrameBuffer();

  void ResetBeforeRender();
  void Set(Shader* shader);
  void Draw();
};

#endif