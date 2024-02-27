#ifndef EQUIRECTANGULAR_MAP_H_
#define EQUIRECTANGULAR_MAP_H_

#include <filesystem>
#include <memory>

#include "frame_buffer_object.h"
#include "shader.h"

class EquirectangularMap {
 private:
  static std::unique_ptr<Shader> kShader;

  uint32_t vao_, vbo_;
  uint32_t width_;
  std::unique_ptr<FrameBufferObject> fbo_;
  Texture equirectangular_texture_;

  void Draw();

 public:
  explicit EquirectangularMap(const std::filesystem::path& path,
                              uint32_t width);

  inline const Texture& cubemap() const { return fbo_->color_texture(0); }
};

#endif