#ifndef EQUIRECTANGULAR_MAP_H_
#define EQUIRECTANGULAR_MAP_H_

#include <filesystem>
#include <memory>

#include "frame_buffer_object.h"
#include "shader.h"

class EquirectangularMap {
 private:
  static std::unique_ptr<Shader> kConvertShader;
  static std::unique_ptr<Shader> kConvolutionShader;
  static std::unique_ptr<Shader> kPrefilterShader;
  static std::unique_ptr<Shader> kLUTShader;
  constexpr static uint32_t kConvolutionResolution = 32;
  constexpr static uint32_t kPrefilterResolution = 128;
  constexpr static uint32_t kPrefilterNumMipLevels = 5;
  constexpr static uint32_t kLUTResolution = 512;

  uint32_t vao_, vbo_;
  uint32_t width_;
  std::unique_ptr<FrameBufferObject> fbo_, convoluted_fbo_, prefiltered_fbo_,
      lut_fbo_;
  Texture equirectangular_texture_;

  void Draw();

 public:
  explicit EquirectangularMap(const std::filesystem::path& path,
                              uint32_t width);

  inline const Texture& environment_map() const {
    return fbo_->color_texture(0);
  }
  inline const Texture& irradiance_map() const {
    return convoluted_fbo_->color_texture(0);
  }
  inline const Texture& prefiltered_map() const {
    return prefiltered_fbo_->color_texture(0);
  }
  inline const Texture& lut() const { return lut_fbo_->color_texture(0); }
};

#endif