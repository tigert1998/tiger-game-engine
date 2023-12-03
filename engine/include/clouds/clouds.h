#ifndef CLOUDS_CLOUDS_H_
#define CLOUDS_CLOUDS_H_

#include <memory>
#include <string>

#include "camera.h"
#include "clouds/noise_texture_generator.h"
#include "frame_buffer_object.h"
#include "light_sources.h"
#include "shader.h"

class Clouds {
 private:
  static const std::string kCsSource;
  static const std::string kFsSource;

  std::unique_ptr<Shader> shader_, screen_space_shader_;

  uint32_t width_, height_;
  uint32_t frag_color_texture_id_;
  std::unique_ptr<FrameBufferObject> fbo_;
  uint32_t vao_;

  std::unique_ptr<NoiseTextureGenerator> noise_texture_generator_;

  void Allocate(uint32_t width, uint32_t height);
  void Deallocate();

 public:
  Clouds(uint32_t width, uint32_t height);

  void Resize(uint32_t width, uint32_t height);

  void Draw(Camera *camera, LightSources *light_sources, double time);

  const FrameBufferObject *fbo() const { return fbo_.get(); }
  void Bind();
  inline void Unbind() { fbo_->Unbind(); }
};

#endif