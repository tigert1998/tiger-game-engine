#ifndef GI_VX_VXGI_CONFIG_H_
#define GI_VX_VXGI_CONFIG_H_

#include <stdint.h>

#include "texture.h"

namespace vxgi {

struct VXGIConfig {
 public:
  bool vxgi_on = true;
  bool direct_lighting_on = true;
  bool indirect_diffuse_lighting_on = true;
  bool indirect_specular_lighting_on = true;
  float step_size = 0.5f;
  float diffuse_offset = 0.5f;
  float diffuse_max_t = 2.0f;
  float specular_aperture = 0.5f;
  float specular_offset = 0.5f;
  float specular_max_t = 2.0f;
  uint32_t voxel_resolution;
  float world_size;
  Texture radiance_map;

  explicit VXGIConfig(float world_size, uint32_t voxel_resolution,
                      const Texture& radiance_map);

  void ImGuiWindow();
};

}  // namespace vxgi

#endif