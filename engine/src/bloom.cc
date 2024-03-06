#include "bloom.h"

#include <glad/glad.h>
#include <imgui.h>

std::unique_ptr<Shader> Bloom::kDownsampleShader = nullptr;
std::unique_ptr<Shader> Bloom::kUpsampleShader = nullptr;
std::unique_ptr<Shader> Bloom::kRenderShader = nullptr;

Bloom::Bloom(uint32_t width, uint32_t height, uint32_t mip_chain_length)
    : mip_chain_length_(mip_chain_length) {
  Resize(width, height);
  glGenVertexArrays(1, &vao_);

  if (kDownsampleShader == nullptr && kUpsampleShader == nullptr &&
      kRenderShader == nullptr) {
    kDownsampleShader = Shader::ScreenSpaceShader("bloom/downsample.frag", {});
    kUpsampleShader = Shader::ScreenSpaceShader("bloom/upsample.frag", {});
    kRenderShader = Shader::ScreenSpaceShader("bloom/render.frag", {});
  }
}

Bloom::~Bloom() { glDeleteVertexArrays(1, &vao_); }

void Bloom::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  {
    auto texture =
        Texture(nullptr, width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(texture));
    input_fbo_.reset(new FrameBufferObject(color_textures));
  }
  CreateMipChain();
}

void Bloom::CreateMipChain() {
  mip_chain_.clear();

  glm::ivec2 size(width_, height_);
  for (int i = 0; i < mip_chain_length_; i++) {
    size /= 2;
    if (size.x == 0 || size.y == 0) break;

    BloomMip mip;
    mip.size = size;
    auto texture = Texture(nullptr, size.x, size.y, GL_RGB16F, GL_RGB, GL_FLOAT,
                           GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(texture));
    mip.fbo.reset(new FrameBufferObject(color_textures));

    mip_chain_.push_back(std::move(mip));
  }
}

void Bloom::RenderDownsamples() {
  glDisable(GL_BLEND);
  kDownsampleShader->Use();

  kDownsampleShader->SetUniformSampler("uSrcTexture",
                                       input_fbo_->color_texture(0), 0);

  for (int i = 0; i < mip_chain_.size(); i++) {
    const auto& mip = mip_chain_[i];
    glViewport(0, 0, mip.size.x, mip.size.y);
    kDownsampleShader->SetUniform<glm::vec4>(
        "uViewport", glm::vec4(0, 0, mip.size.x, mip.size.y));
    mip.fbo->Bind();
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    mip.fbo->Unbind();

    kDownsampleShader->SetUniformSampler("uSrcTexture",
                                         mip.fbo->color_texture(0), 0);
  }
}

void Bloom::RenderUpsamples() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glBlendEquation(GL_FUNC_ADD);

  kUpsampleShader->Use();
  kUpsampleShader->SetUniform<float>("uFilterRadius", filter_radius_);

  for (int i = mip_chain_.size() - 1; i > 0; i--) {
    const auto& mip = mip_chain_[i];
    const auto& next_mip = mip_chain_[i - 1];

    kUpsampleShader->SetUniformSampler("uSrcTexture", mip.fbo->color_texture(0),
                                       0);

    glViewport(0, 0, next_mip.size.x, next_mip.size.y);
    kUpsampleShader->SetUniform<glm::vec4>(
        "uViewport", glm::vec4(0, 0, next_mip.size.x, next_mip.size.y));

    next_mip.fbo->Bind();
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    next_mip.fbo->Unbind();
  }

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Bloom::Draw(const FrameBufferObject* dest_fbo) {
  RenderDownsamples();
  RenderUpsamples();

  if (dest_fbo != nullptr) dest_fbo->Bind();

  glViewport(0, 0, width_, height_);
  kRenderShader->Use();
  kRenderShader->SetUniformSampler("uScene", input_fbo_->color_texture(0), 0);
  kRenderShader->SetUniformSampler("uBloomBlur",
                                   mip_chain_[0].fbo->color_texture(0), 1);
  kRenderShader->SetUniform<glm::vec4>("uViewport",
                                       glm::vec4(0, 0, width_, height_));
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);

  if (dest_fbo != nullptr) dest_fbo->Unbind();
}

void Bloom::ImGuiWindow() {
  ImGui::Begin("Bloom:");
  ImGui::InputFloat("Filter Radius", &filter_radius_);
  ImGui::End();
}