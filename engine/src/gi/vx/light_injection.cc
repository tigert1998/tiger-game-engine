#include "gi/vx/light_injection.h"

#include "utils.h"

std::unique_ptr<Shader> LightInjection::kInjectionShader = nullptr;

LightInjection::LightInjection(float world_size, uint32_t voxel_resolution)
    : world_size_(world_size), voxel_resolution_(voxel_resolution) {
  if (kInjectionShader == nullptr) {
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
  }

  texture_ =
      Texture(nullptr, GL_TEXTURE_3D, voxel_resolution_, voxel_resolution_,
              voxel_resolution_, GL_RGBA16F, GL_RGBA, GL_FLOAT,
              GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST, {}, false);
}

void LightInjection::Launch(const Texture& albedo, const Texture& normal,
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
  kInjectionShader->SetUniform<uint32_t>("uVoxelResolution", voxel_resolution_);
  kInjectionShader->SetUniform<float>("uWorldSize", world_size_);
  kInjectionShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  glDispatchCompute((voxel_resolution_ + 7) / 8, (voxel_resolution_ + 7) / 8,
                    (voxel_resolution_ + 7) / 8);
}