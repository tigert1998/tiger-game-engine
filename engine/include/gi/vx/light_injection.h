#ifndef GI_VX_LIGHT_INJECTION_H_
#define GI_VX_LIGHT_INJECTION_H_

#include <stdint.h>

#include "camera.h"
#include "light_sources.h"
#include "shader.h"
#include "texture.h"

namespace vxgi {

class LightInjection {
 private:
  static std::unique_ptr<Shader> kInjectionShader;
  static std::unique_ptr<Shader> kMipmapShader;

  float world_size_;
  uint32_t voxel_resolution_;

  Texture texture_;

 public:
  explicit LightInjection(float world_size, uint32_t voxel_resolution);

  void Launch(const Texture& albedo, const Texture& normal,
              const Texture& metallic_and_roughness, const Camera* camera,
              const LightSources* light_sources);

  inline const Texture& light_injected() { return texture_; }
};

}  // namespace vxgi

#endif