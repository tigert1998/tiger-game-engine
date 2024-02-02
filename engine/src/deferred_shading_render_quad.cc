#include "deferred_shading_render_quad.h"

#include <glad/glad.h>

#include <random>

#include "utils.h"

void DeferredShadingRenderQuad::InitSSAO() {
  std::uniform_real_distribution<float> dis(0.0, 1.0);
  std::default_random_engine generator;
  std::vector<glm::vec4> ssao_kernel;
  for (unsigned int i = 0; i < 64; ++i) {
    glm::vec3 sample(dis(generator) * 2.0 - 1.0, dis(generator) * 2.0 - 1.0,
                     dis(generator));
    sample = glm::normalize(sample);
    sample *= dis(generator);
    float scale = float(i) / 64.0f;
    scale = glm::mix(0.1f, 1.0f, scale * scale);
    sample *= scale;
    ssao_kernel.push_back(glm::vec4(sample, 0));
  }
  ssao_kernel_ssbo_.reset(new SSBO(ssao_kernel.size() * sizeof(ssao_kernel[0]),
                                   ssao_kernel.data(), GL_STATIC_DRAW, 0));

  std::vector<glm::vec3> ssao_noise;
  for (unsigned int i = 0; i < 16; i++) {
    glm::vec3 noise(dis(generator) * 2.0 - 1.0, dis(generator) * 2.0 - 1.0,
                    0.0f);
    ssao_noise.push_back(noise);
  }

  ssao_noise_texture_ =
      Texture(ssao_noise.data(), 4, 4, GL_RGBA16F, GL_RGB, GL_FLOAT, GL_REPEAT,
              GL_NEAREST, GL_NEAREST, {}, false);
}

void DeferredShadingRenderQuad::ClearTextures() {
  uint8_t zero = 0;
  glClearTexImage(fbo_->color_texture(7).id(), 0, GL_RED_INTEGER,
                  GL_UNSIGNED_BYTE, &zero);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void DeferredShadingRenderQuad::Allocate(uint32_t width, uint32_t height) {
  Texture ka(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
             GL_NEAREST, GL_NEAREST, {}, false);
  Texture kd(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
             GL_NEAREST, GL_NEAREST, {}, false);
  Texture ks_and_shininess(nullptr, width, height, GL_RGBA16F, GL_RGBA,
                           GL_FLOAT, GL_REPEAT, GL_NEAREST, GL_NEAREST, {},
                           false);
  Texture albedo(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture metallic_and_roughness_and_ao(nullptr, width, height, GL_RGB16F,
                                        GL_RGB, GL_FLOAT, GL_REPEAT, GL_NEAREST,
                                        GL_NEAREST, {}, false);
  Texture normal(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture position_and_alpha(nullptr, width, height, GL_RGBA16F, GL_RGBA,
                             GL_FLOAT, GL_REPEAT, GL_NEAREST, GL_NEAREST, {},
                             false);
  Texture flag(nullptr, width, height, GL_R8UI, GL_RED_INTEGER,
               GL_UNSIGNED_BYTE, GL_REPEAT, GL_NEAREST, GL_NEAREST, {}, false);
  std::vector<Texture> color_textures;
  color_textures.push_back(std::move(ka));
  color_textures.push_back(std::move(kd));
  color_textures.push_back(std::move(ks_and_shininess));
  color_textures.push_back(std::move(albedo));
  color_textures.push_back(std::move(metallic_and_roughness_and_ao));
  color_textures.push_back(std::move(normal));
  color_textures.push_back(std::move(position_and_alpha));
  color_textures.push_back(std::move(flag));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(color_textures, depth_texture));

  // SSAO
  Texture ssao_color_texture(nullptr, width, height, GL_RED, GL_RED, GL_FLOAT,
                             GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST, {},
                             false);
  std::vector<Texture> ssao_color_textures;
  ssao_color_textures.push_back(std::move(ssao_color_texture));
  ssao_fbo_.reset(new FrameBufferObject(ssao_color_textures));

  Texture ssao_blur_color_texture(nullptr, width, height, GL_RED, GL_RED,
                                  GL_FLOAT, GL_CLAMP_TO_EDGE, GL_NEAREST,
                                  GL_NEAREST, {}, false);
  std::vector<Texture> ssao_blur_color_textures;
  ssao_blur_color_textures.push_back(std::move(ssao_blur_color_texture));
  ssao_blur_fbo_.reset(new FrameBufferObject(ssao_blur_color_textures));
}

DeferredShadingRenderQuad::DeferredShadingRenderQuad(uint32_t width,
                                                     uint32_t height) {
  InitSSAO();
  Resize(width, height);

  if (kShader == nullptr && kSSAOShader == nullptr &&
      kSSAOBlurShader == nullptr) {
    std::map<std::string, std::any> defines = {
        {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
    };
    kShader = Shader::ScreenSpaceShader(
        "deferred_shading/deferred_shading.frag", defines);
    kSSAOShader = Shader::ScreenSpaceShader("deferred_shading/ssao.frag", {});
    kSSAOBlurShader =
        Shader::ScreenSpaceShader("deferred_shading/ssao_blur.frag", {});
    glGenVertexArrays(1, &vao_);
  }
}

void DeferredShadingRenderQuad::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  Allocate(width, height);
}

void DeferredShadingRenderQuad::TwoPasses(
    const Camera* camera, LightSources* light_sources,
    ShadowSources* shadow_sources, bool enable_ssao,
    const std::function<void()>& first_pass,
    const std::function<void()>& second_pass,
    const FrameBufferObject* dest_fbo) {
  if (dest_fbo != nullptr) {
    dest_fbo->Bind();
  }
  glViewport(0, 0, width_, height_);
  first_pass();
  if (dest_fbo != nullptr) {
    dest_fbo->Unbind();
  }

  glDisable(GL_BLEND);
  fbo_->Bind();
  ClearTextures();
  glViewport(0, 0, width_, height_);
  // Draw objects into G-Buffer
  second_pass();
  fbo_->Unbind();
  glEnable(GL_BLEND);

  if (enable_ssao) {
    glDisable(GL_BLEND);
    // SSAO
    ssao_fbo_->Bind();
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vao_);
    kSSAOShader->Use();
    kSSAOShader->SetUniformSampler("normal", fbo_->color_texture(5), 0);
    kSSAOShader->SetUniformSampler("positionAndAlpha", fbo_->color_texture(6),
                                   1);
    kSSAOShader->SetUniformSampler("uNoiseTexture", ssao_noise_texture_, 2);
    kSSAOShader->SetUniform<glm::vec2>("uScreenSize",
                                       glm::vec2(width_, height_));
    kSSAOShader->SetUniform<float>("uRadius", 0.5);
    kSSAOShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
    kSSAOShader->SetUniform<glm::mat4>("uProjectionMatrix",
                                       camera->projection_matrix());
    ssao_kernel_ssbo_->BindBufferBase();
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    ssao_fbo_->Unbind();

    // SSAO blur
    ssao_blur_fbo_->Bind();
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vao_);
    kSSAOBlurShader->Use();
    kSSAOBlurShader->SetUniform<glm::vec2>("uScreenSize",
                                           glm::vec2(width_, height_));
    kSSAOBlurShader->SetUniformSampler("uInput", ssao_fbo_->color_texture(0),
                                       0);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    ssao_blur_fbo_->Unbind();
    glEnable(GL_BLEND);
  }

  glBindVertexArray(vao_);
  kShader->Use();
  kShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  kShader->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  kShader->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));
  kShader->SetUniform<int32_t>("uEnableSSAO", enable_ssao);

  kShader->SetUniformSampler("ka", fbo_->color_texture(0), 0);
  kShader->SetUniformSampler("kd", fbo_->color_texture(1), 1);
  kShader->SetUniformSampler("ksAndShininess", fbo_->color_texture(2), 2);
  kShader->SetUniformSampler("albedo", fbo_->color_texture(3), 3);
  kShader->SetUniformSampler("metallicAndRoughnessAndAo",
                             fbo_->color_texture(4), 4);
  kShader->SetUniformSampler("normal", fbo_->color_texture(5), 5);
  kShader->SetUniformSampler("positionAndAlpha", fbo_->color_texture(6), 6);
  kShader->SetUniformSampler("flag", fbo_->color_texture(7), 7);
  kShader->SetUniformSampler("depth", fbo_->depth_texture(), 8);

  if (enable_ssao) {
    kShader->SetUniformSampler("uSSAO", ssao_blur_fbo_->color_texture(0), 9);
  } else {
    kShader->SetUniformSampler("uSSAO", Texture::Empty(GL_TEXTURE_2D), 9);
  }

  int num_samplers = 10;

  light_sources->Set(kShader.get(), false);
  shadow_sources->Set(kShader.get(), &num_samplers);

  if (dest_fbo != nullptr) {
    dest_fbo->Bind();
  }
  glDrawArrays(GL_POINTS, 0, 1);
  if (dest_fbo != nullptr) {
    dest_fbo->Unbind();
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

uint32_t DeferredShadingRenderQuad::vao_ = 0;

std::unique_ptr<Shader> DeferredShadingRenderQuad::kShader = nullptr;
std::unique_ptr<Shader> DeferredShadingRenderQuad::kSSAOShader = nullptr;
std::unique_ptr<Shader> DeferredShadingRenderQuad::kSSAOBlurShader = nullptr;
