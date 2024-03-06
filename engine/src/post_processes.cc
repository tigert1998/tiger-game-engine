#include "post_processes.h"

#include <glad/glad.h>

#include <glm/glm.hpp>

std::unique_ptr<Shader> ToneMappingAndGammaCorrection::kShader = nullptr;

ToneMappingAndGammaCorrection::ToneMappingAndGammaCorrection(uint32_t width,
                                                             uint32_t height) {
  Resize(width, height);
  glGenVertexArrays(1, &vao_);
  if (kShader == nullptr) {
    kShader =
        Shader::ScreenSpaceShader("tone_mapping_and_gamma_correction.frag", {});
  }
}

ToneMappingAndGammaCorrection::~ToneMappingAndGammaCorrection() {
  glDeleteVertexArrays(1, &vao_);
}

void ToneMappingAndGammaCorrection::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  auto texture = Texture(nullptr, width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                         GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
  std::vector<Texture> color_textures;
  color_textures.push_back(std::move(texture));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  input_fbo_.reset(new FrameBufferObject(color_textures, depth_texture));
}

const FrameBufferObject *ToneMappingAndGammaCorrection::fbo() const {
  return input_fbo_.get();
}

void ToneMappingAndGammaCorrection::Draw(const FrameBufferObject *dest_fbo) {
  if (dest_fbo != nullptr) dest_fbo->Bind();

  kShader->Use();
  kShader->SetUniform<glm::vec4>("uViewport", glm::vec4(0, 0, width_, height_));
  kShader->SetUniform<float>("uAdaptedLum", adapted_lum_);
  kShader->SetUniformSampler("uScene", input_fbo_->color_texture(0), 0);
  glViewport(0, 0, width_, height_);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);

  if (dest_fbo != nullptr) dest_fbo->Unbind();
}

void ToneMappingAndGammaCorrection::ImGuiWindow() {}

uint32_t PostProcesses::Size() const { return post_processes_.size(); }

void PostProcesses::Enable(uint32_t index, bool enabled) {
  enabled_[index] = enabled;
}

void PostProcesses::Add(std::unique_ptr<PostProcess> post_process) {
  post_processes_.push_back(std::move(post_process));
  enabled_.push_back(true);
}

void PostProcesses::Resize(uint32_t width, uint32_t height) {
  for (const auto &post_process : post_processes_) {
    post_process->Resize(width, height);
  }
}

const FrameBufferObject *PostProcesses::fbo() const {
  for (int i = 0; i < post_processes_.size(); i++) {
    if (enabled_[i]) return post_processes_[i]->fbo();
  }
  return nullptr;
}

void PostProcesses::Draw(const FrameBufferObject *dest_fbo) {
  std::vector<PostProcess *> tmp;
  tmp.reserve(post_processes_.size());
  for (int i = 0; i < post_processes_.size(); i++) {
    if (enabled_[i]) tmp.push_back(post_processes_[i].get());
  }

  for (int i = 0; i < tmp.size(); i++) {
    auto cur_fbo = i + 1 < tmp.size() ? tmp[i + 1]->fbo() : dest_fbo;
    tmp[i]->Draw(cur_fbo);
  }
}

void PostProcesses::ImGuiWindow() {
  for (int i = 0; i < post_processes_.size(); i++) {
    if (!enabled_[i]) continue;
    post_processes_[i]->ImGuiWindow();
  }
}
