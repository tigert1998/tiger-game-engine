#ifndef DEFERRED_SHADING_RENDER_QUAD_H_
#define DEFERRED_SHADING_RENDER_QUAD_H_

#include <functional>
#include <memory>

#include "camera.h"
#include "frame_buffer_object.h"
#include "light_sources.h"
#include "shader.h"
#include "shadow_sources.h"
#include "ssbo.h"

class DeferredShadingRenderQuad {
 private:
  std::unique_ptr<FrameBufferObject> fbo_;
  uint32_t width_, height_;

  void ClearTextures();
  void Allocate(uint32_t width, uint32_t height);
  void InitSSAO();

  std::unique_ptr<SSBO> ssao_kernel_ssbo_;
  Texture ssao_noise_texture_;
  std::unique_ptr<FrameBufferObject> ssao_fbo_, ssao_blur_fbo_;

  static const std::string kFsSource, kSSAOFsSource, kSSAOBlurFsSource;
  static std::unique_ptr<Shader> kShader, kSSAOShader, kSSAOBlurShader;
  static uint32_t vao_;

 public:
  explicit DeferredShadingRenderQuad(uint32_t width, uint32_t height);

  void Resize(uint32_t width, uint32_t height);

  void TwoPasses(const Camera* camera, LightSources* light_sources,
                 ShadowSources* shadow_sources, bool enable_ssao,
                 const std::function<void()>& first_pass,
                 const std::function<void()>& second_pass);
};

#endif