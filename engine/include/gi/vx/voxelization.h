#ifndef GI_VX_VOXELIZATION_H_
#define GI_VX_VOXELIZATION_H_

#include <glm/glm.hpp>

#include "shader.h"
#include "texture.h"

namespace vxgi {

class Voxelization {
 private:
  glm::mat4 view_projection_matrix_x, view_projection_matrix_y,
      view_projection_matrix_z;
  uint32_t voxel_resolution_;
  float world_size_;
  Texture albedo_texture_, normal_texture_, metallic_and_roughness_texture_;

 public:
  explicit Voxelization(float world_size, uint32_t voxel_resolution);

  void Set(Shader *shader);

  inline uint32_t voxel_resolution() const { return voxel_resolution_; }
  inline uint32_t world_size() const { return world_size_; }

  inline const Texture &albedo() { return albedo_texture_; }
  inline const Texture &normal() { return normal_texture_; }
  inline const Texture &metallic_and_roughness() {
    return metallic_and_roughness_texture_;
  }
};

}  // namespace vxgi

#endif