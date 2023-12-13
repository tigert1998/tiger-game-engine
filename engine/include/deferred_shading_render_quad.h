#ifndef DEFERRED_SHADING_RENDER_QUAD_H_
#define DEFERRED_SHADING_RENDER_QUAD_H_

#include <functional>
#include <memory>

#include "camera.h"
#include "frame_buffer_object.h"
#include "light_sources.h"
#include "shader.h"
#include "shadow_sources.h"

class DeferredShadingRenderQuad {
 private:
  std::unique_ptr<FrameBufferObject> fbo_;
  uint32_t width_, height_;

  void ClearTextures();
  void Allocate(uint32_t width, uint32_t height);

  static const std::string kFsSource;
  static std::shared_ptr<Shader> kShader;
  static uint32_t vao_;

  std::shared_ptr<Shader> shader_;

 public:
  explicit DeferredShadingRenderQuad(uint32_t width, uint32_t height);

  void Resize(uint32_t width, uint32_t height);

  void TwoPasses(const Camera* camera, LightSources* light_sources,
                 ShadowSources* shadow_sources,
                 const std::function<void()>& first_pass,
                 const std::function<void()>& second_pass);
};

#endif