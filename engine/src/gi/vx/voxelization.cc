#include "gi/vx/voxelization.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

namespace vxgi {

Voxelization::Voxelization(float world_size, uint32_t voxel_resolution)
    : world_size_(world_size), voxel_resolution_(voxel_resolution) {
  auto projection =
      glm::ortho(-world_size * 0.5f, world_size * 0.5f, -world_size * 0.5f,
                 world_size * 0.5f, world_size * 0.5f, world_size * 1.5f);
  view_projection_matrix_x =
      projection * glm::lookAt(glm::vec3(world_size, 0, 0), glm::vec3(0, 0, 0),
                               glm::vec3(0, 1, 0));
  view_projection_matrix_y =
      projection * glm::lookAt(glm::vec3(0, world_size, 0), glm::vec3(0, 0, 0),
                               glm::vec3(0, 0, -1));
  view_projection_matrix_z =
      projection * glm::lookAt(glm::vec3(0, 0, world_size), glm::vec3(0, 0, 0),
                               glm::vec3(0, 1, 0));

  albedo_texture_ =
      Texture(nullptr, GL_TEXTURE_3D, voxel_resolution_, voxel_resolution_,
              voxel_resolution_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT,
              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
  normal_texture_ =
      Texture(nullptr, GL_TEXTURE_3D, voxel_resolution_, voxel_resolution_,
              voxel_resolution_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT,
              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
  metallic_and_roughness_texture_ =
      Texture(nullptr, GL_TEXTURE_3D, voxel_resolution_, voxel_resolution_,
              voxel_resolution_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT,
              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
}

void Voxelization::Set(Shader *shader) {
  shader->SetUniform<glm::mat4>("uViewProjectionMatrixX",
                                view_projection_matrix_x);
  shader->SetUniform<glm::mat4>("uViewProjectionMatrixY",
                                view_projection_matrix_y);
  shader->SetUniform<glm::mat4>("uViewProjectionMatrixZ",
                                view_projection_matrix_z);
  shader->SetUniform<uint32_t>("uVoxelResolution", voxel_resolution_);

  uint32_t zero = 0;
  glClearTexImage(albedo_texture_.id(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
                  &zero);
  glClearTexImage(normal_texture_.id(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
                  &zero);

  glBindImageTexture(0, albedo_texture_.id(), 0, false, 0, GL_READ_WRITE,
                     GL_R32UI);
  shader->SetUniform<int32_t>("uAlbedoImage", 0);
  glBindImageTexture(1, normal_texture_.id(), 0, false, 0, GL_READ_WRITE,
                     GL_R32UI);
  shader->SetUniform<int32_t>("uNormalImage", 1);
  glBindImageTexture(2, metallic_and_roughness_texture_.id(), 0, false, 0,
                     GL_READ_WRITE, GL_R32UI);
  shader->SetUniform<int32_t>("uMetallicAndRoughnessImage", 2);
}

}  // namespace vxgi