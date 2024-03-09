#include "gi/vx/light_injection.h"

#include "utils.h"

namespace vxgi {

std::unique_ptr<Shader> LightInjection::kInjectionShader = nullptr;
std::unique_ptr<Shader> LightInjection::kMipmapShader = nullptr;

LightInjection::LightInjection(float world_size, uint32_t voxel_resolution)
    : world_size_(world_size), voxel_resolution_(voxel_resolution) {
  if (kInjectionShader == nullptr && kMipmapShader == nullptr) {
    kInjectionShader.reset(new Shader(
        {{GL_COMPUTE_SHADER, "vxgi/light_injection.comp"}},
        {{"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
         {"AMBIENT_LIGHT_BINDING", std::any(AmbientLight::GLSL_BINDING)},
         {"DIRECTIONAL_LIGHT_BINDING",
          std::any(DirectionalLight::GLSL_BINDING)},
         {"POINT_LIGHT_BINDING", std::any(PointLight::GLSL_BINDING)},
         {"IMAGE_BASED_LIGHT_BINDING", std::any(ImageBasedLight::GLSL_BINDING)},
         {"POISSON_DISK_2D_BINDING",
          std::any(LightSources::POISSON_DISK_2D_BINDING)}}));

    kMipmapShader.reset(
        new Shader({{GL_COMPUTE_SHADER, "vxgi/mipmap.comp"}}, {}));
  }

  texture_ = Texture(nullptr, GL_TEXTURE_3D, voxel_resolution_,
                     voxel_resolution_, voxel_resolution_, GL_RGBA16F, GL_RGBA,
                     GL_FLOAT, GL_CLAMP_TO_EDGE, GL_NEAREST_MIPMAP_NEAREST,
                     GL_NEAREST, {}, true);
}

void LightInjection::Launch(const Texture& albedo, const Texture& normal,
                            const Texture& metallic_and_roughness,
                            const Camera* camera,
                            const LightSources* light_sources) {
  kInjectionShader->Use();
  glClearTexImage(texture_.id(), 0, GL_RGBA, GL_FLOAT, nullptr);
  glBindImageTexture(0, texture_.id(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA16F);
  light_sources->Set(kInjectionShader.get());
  kInjectionShader->SetUniform<int32_t>("uInjected", 0);
  kInjectionShader->SetUniformSampler("uAlbedo", albedo, 0);
  kInjectionShader->SetUniformSampler("uNormal", normal, 1);
  kInjectionShader->SetUniformSampler("uMetallicAndRoughness",
                                      metallic_and_roughness, 2);
  kInjectionShader->SetUniform<uint32_t>("uVoxelResolution", voxel_resolution_);
  kInjectionShader->SetUniform<float>("uWorldSize", world_size_);
  kInjectionShader->SetUniform<glm::vec3>("uCameraPosition",
                                          camera->position());
  kInjectionShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  glDispatchCompute((voxel_resolution_ + 7) / 8, (voxel_resolution_ + 7) / 8,
                    (voxel_resolution_ + 7) / 8);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  kMipmapShader->Use();
  const uint32_t num_levels = 1000;
  uint32_t resolution = voxel_resolution_;
  for (int level = 1; level < num_levels; level++) {
    resolution /= 2;
    if (resolution == 0) break;

    glBindImageTexture(0, texture_.id(), level, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA16F);
    kMipmapShader->SetUniform<int32_t>("uInjected", 0);
    glBindImageTexture(1, texture_.id(), level - 1, GL_FALSE, 0, GL_READ_ONLY,
                       GL_RGBA16F);
    kMipmapShader->SetUniform<int32_t>("uLastInjected", 1);
    kMipmapShader->SetUniform<uint32_t>("uVoxelResolution", resolution);
    uint32_t num_work_groups = (resolution + 7) / 8;
    glDispatchCompute(num_work_groups, num_work_groups, num_work_groups);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  }
}

}  // namespace vxgi