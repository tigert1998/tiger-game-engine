#ifndef DEFERRED_SHADING_RENDER_QUAD_H_
#define DEFERRED_SHADING_RENDER_QUAD_H_

#include <functional>
#include <memory>

#include "camera.h"
#include "frame_buffer_object.h"
#include "gi/vx/vxgi_config.h"
#include "light_sources.h"
#include "ogl_buffer.h"
#include "shader.h"
#include "shadows/directional_shadow.h"
#include "shadows/shadow.h"

class DeferredShadingRenderQuad {
 private:
  std::unique_ptr<FrameBufferObject> fbo_;
  uint32_t width_, height_;

  void ClearTextures();
  void Allocate(uint32_t width, uint32_t height);
  void InitSSAO();

  std::unique_ptr<OGLBuffer> ssao_kernel_ssbo_;
  Texture ssao_noise_texture_;
  std::unique_ptr<FrameBufferObject> ssao_fbo_, ssao_blur_fbo_;

  static std::unique_ptr<Shader> kShader, kSSAOShader, kSSAOBlurShader;
  static uint32_t vao_;

 public:
  explicit DeferredShadingRenderQuad(uint32_t width, uint32_t height);

  void Resize(uint32_t width, uint32_t height);

  void TwoPasses(const Camera* camera, LightSources* light_sources,
                 bool enable_ssao, vxgi::VXGIConfig* vxgi_config,
                 const std::function<void()>& first_pass,
                 const std::function<void()>& second_pass,
                 const FrameBufferObject* dest_fbo);
};

#endif