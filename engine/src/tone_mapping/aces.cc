#include "tone_mapping/aces.h"

#include <glad/glad.h>

#include <glm/glm.hpp>

namespace tone_mapping {

std::unique_ptr<Shader> ACES::kShader = nullptr;

ACES::ACES(uint32_t width, uint32_t height) {
  Resize(width, height);
  glGenVertexArrays(1, &vao_);
  if (kShader == nullptr) {
    kShader = Shader::ScreenSpaceShader("tone_mapping/aces/pass.frag", {});
  }
}

ACES::~ACES() { glDeleteVertexArrays(1, &vao_); }

void ACES::Resize(uint32_t width, uint32_t height) {
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

const FrameBufferObject *ACES::fbo() const { return input_fbo_.get(); }

void ACES::Draw(const FrameBufferObject *dest_fbo) {
  if (dest_fbo != nullptr) dest_fbo->Bind();

  kShader->Use();
  kShader->SetUniform<glm::vec4>("uViewport", glm::vec4(0, 0, width_, height_));
  kShader->SetUniformSampler("uScene", input_fbo_->color_texture(0), 0);
  kShader->SetUniform<float>("uAdaptedLum", adapted_lum_);
  glViewport(0, 0, width_, height_);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);

  if (dest_fbo != nullptr) dest_fbo->Unbind();
}

void ACES::ImGuiWindow() {}

}  // namespace tone_mapping