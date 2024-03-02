#include "clouds/clouds.h"

#include <glad/glad.h>

#include "light_sources.h"
#include "utils.h"

void Clouds::Bind() {
  glViewport(0, 0, width_, height_);
  fbo_->Bind();
}

void Clouds::Allocate(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  frag_color_texture_ =
      Texture(nullptr, width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_REPEAT,
              GL_LINEAR, GL_LINEAR, {}, false);

  std::vector<Texture> color_textures;
  color_textures.push_back(Texture(nullptr, width, height, GL_RGBA, GL_RGBA,
                                   GL_UNSIGNED_BYTE, GL_REPEAT, GL_LINEAR,
                                   GL_LINEAR, {}, false));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(color_textures, depth_texture));
}

void Clouds::Deallocate() { frag_color_texture_.Clear(); }

Clouds::Clouds(uint32_t width, uint32_t height) {
  noise_texture_generator_.reset(new NoiseTextureGenerator(0.8));
  shader_.reset(new Shader(
      {{GL_COMPUTE_SHADER, "clouds/clouds.comp"}},
      {
          {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
          {"AMBIENT_LIGHT_BINDING", std::any(AmbientLight::GLSL_BINDING)},
          {"DIRECTIONAL_LIGHT_BINDING",
           std::any(DirectionalLight::GLSL_BINDING)},
          {"POINT_LIGHT_BINDING", std::any(PointLight::GLSL_BINDING)},
          {"IMAGE_BASED_LIGHT_BINDING",
           std::any(ImageBasedLight::GLSL_BINDING)},
          {"POISSON_DISK_2D_BINDING",
           std::any(LightSources::POISSON_DISK_2D_BINDING)},
      }));
  screen_space_shader_ = Shader::ScreenSpaceShader("clouds/clouds.frag", {});
  glGenVertexArrays(1, &vao_);
  Allocate(width, height);
}

void Clouds::Resize(uint32_t width, uint32_t height) {
  Deallocate();
  Allocate(width, height);
}

void Clouds::Draw(Camera *camera, LightSources *light_sources, double time) {
  shader_->Use();
  light_sources->Set();
  glBindImageTexture(0, frag_color_texture_.id(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA32F);
  shader_->SetUniform<int32_t>("uFragColor", 0);
  shader_->SetUniformSampler("uPerlinWorleyTexture",
                             noise_texture_generator_->perlin_worley_texture(),
                             0);
  shader_->SetUniformSampler("uWorleyTexture",
                             noise_texture_generator_->worley_texture(), 1);
  shader_->SetUniformSampler("uWeatherTexture",
                             noise_texture_generator_->weather_texture(), 2);
  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));
  shader_->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  shader_->SetUniform<glm::mat4>(
      "uInvViewProjectionMatrix",
      glm::inverse(camera->projection_matrix() * camera->view_matrix()));
  shader_->SetUniform<float>("uTime", time);
  glDispatchCompute((width_ + 3) / 4, (height_ + 3) / 4, 1);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_3D, 0);

  glBindVertexArray(vao_);
  screen_space_shader_->Use();
  screen_space_shader_->SetUniformSampler("uScreenTexture",
                                          fbo_->color_texture(0), 0);
  screen_space_shader_->SetUniformSampler("uCloudTexture", frag_color_texture_,
                                          1);
  screen_space_shader_->SetUniformSampler("uDepthTexture",
                                          fbo_->depth_texture(), 2);
  screen_space_shader_->SetUniform<glm::vec2>("uScreenSize",
                                              glm::vec2(width_, height_));
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}
