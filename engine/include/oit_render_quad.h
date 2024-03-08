#ifndef OIT_RENDER_QUAD_H_
#define OIT_RENDER_QUAD_H_

#include <glad/glad.h>
#include <stdint.h>

#include <functional>
#include <memory>

#include "frame_buffer_object.h"
#include "ogl_buffer.h"
#include "shader.h"
#include "texture.h"

class OITRenderQuad {
 private:
  uint32_t width_, height_;
  Texture head_pointer_texture_;
  std::unique_ptr<OGLBuffer> head_pointer_initializer_, atomic_counter_buffer_,
      fragment_storage_buffer_;
  uint32_t fragment_storage_texture_;
  std::unique_ptr<FrameBufferObject> fbo_;

  void Deallocate();
  void Allocate(uint32_t width, uint32_t height, float fragment_per_pixel);

  static std::shared_ptr<Shader> kShader;
  static uint32_t vao_;

  std::shared_ptr<Shader> shader_;

  void ResetBeforeRender();
  void Draw();

 public:
  OITRenderQuad(uint32_t width, uint32_t height);
  void Resize(uint32_t width, uint32_t height);

  void Set(Shader* shader);

  // Please use this API for rendering
  void TwoPasses(const std::function<void()>& first_pass,
                 const std::function<void()>& second_pass,
                 const FrameBufferObject* dest_fbo);
};

#endif