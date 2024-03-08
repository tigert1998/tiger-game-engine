#include "oit_render_quad.h"

#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

#include "utils.h"

void OITRenderQuad::Allocate(uint32_t width, uint32_t height,
                             float fragment_per_pixel) {
  uint32_t total_pixels = width * height;

  head_pointer_texture_ =
      Texture(nullptr, width, height, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT,
              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);

  head_pointer_initializer_.reset(new OGLBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                total_pixels * sizeof(uint32_t),
                                                nullptr, GL_STATIC_DRAW, -1));
  head_pointer_initializer_->Bind();
  uint32_t *data = (uint32_t *)head_pointer_initializer_->Map(GL_WRITE_ONLY);
  std::fill(data, data + total_pixels, 0xFFFFFFFF);
  head_pointer_initializer_->Unmap();
  head_pointer_initializer_->Unbind();

  atomic_counter_buffer_.reset(new OGLBuffer(
      GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY, 0));

  fragment_storage_buffer_.reset(new OGLBuffer(
      GL_TEXTURE_BUFFER,
      (size_t)ceil(fragment_per_pixel * total_pixels) * sizeof(glm::uvec4),
      nullptr, GL_DYNAMIC_COPY, -1));

  glGenTextures(1, &fragment_storage_texture_);

  std::vector<Texture> color_textures;
  color_textures.push_back(Texture(nullptr, width, height, GL_RGBA, GL_RGBA,
                                   GL_UNSIGNED_BYTE, GL_REPEAT, GL_LINEAR,
                                   GL_LINEAR, {}, false));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(color_textures, depth_texture));
}

void OITRenderQuad::Deallocate() {
  glDeleteTextures(1, &fragment_storage_texture_);
}

OITRenderQuad::OITRenderQuad(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
  Allocate(width, height, 16);

  if (kShader == nullptr) {
    kShader = Shader::ScreenSpaceShader("oit/oit.frag", {});
    glGenVertexArrays(1, &vao_);
  }
  shader_ = kShader;
}

void OITRenderQuad::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  Deallocate();
  Allocate(width, height, 16);
}

void OITRenderQuad::ResetBeforeRender() {
  head_pointer_initializer_->Bind();
  glBindTexture(head_pointer_texture_.target(), head_pointer_texture_.id());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width_, height_, 0, GL_RED_INTEGER,
               GL_UNSIGNED_INT, nullptr);
  head_pointer_initializer_->Unbind();
  glBindImageTexture(0,  // unit
                     head_pointer_texture_.id(), 0, false,
                     0,  // level, not layered, layer
                     GL_READ_WRITE, GL_R32UI);

  atomic_counter_buffer_->BindBufferBase();
  const uint32_t zero = 0;
  atomic_counter_buffer_->SubData(0, sizeof(zero), &zero);

  glBindTexture(GL_TEXTURE_BUFFER, fragment_storage_texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, fragment_storage_buffer_->id());
  glBindImageTexture(1, fragment_storage_texture_, 0, false, 0, GL_READ_WRITE,
                     GL_RGBA32UI);
}

void OITRenderQuad::Set(Shader *shader) {
  shader->SetUniform<int32_t>("uHeadPointers", 0);
  atomic_counter_buffer_->BindBufferBase();
  // uListSize must have binding = 0
  shader->SetUniform<int32_t>("uList", 1);
}

void OITRenderQuad::Draw() {
  glDepthFunc(GL_ALWAYS);

  glBindVertexArray(vao_);
  shader_->Use();

  shader_->SetUniformSampler("uHeadPointers", head_pointer_texture_, 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_BUFFER, fragment_storage_texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, fragment_storage_buffer_->id());
  shader_->SetUniform<int32_t>("uList", 1);

  shader_->SetUniformSampler("uBackground", fbo_->color_texture(0), 2);
  shader_->SetUniformSampler("uBackgroundDepth", fbo_->depth_texture(), 3);

  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));

  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  glDepthFunc(GL_LESS);
}

void OITRenderQuad::TwoPasses(const std::function<void()> &first_pass,
                              const std::function<void()> &second_pass,
                              const FrameBufferObject *dest_fbo) {
  glViewport(0, 0, width_, height_);
  fbo_->Bind();
  // draw background for OIT render quad
  // all opaque objects must be draw here
  first_pass();
  fbo_->Unbind();

  // append transparent objects to the linked list
  glViewport(0, 0, width_, height_);
  glDepthMask(GL_FALSE);
  ResetBeforeRender();
  second_pass();
  glDepthMask(GL_TRUE);

  if (dest_fbo != nullptr) {
    dest_fbo->Bind();
  }
  // draw OIT render quad
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  Draw();
  if (dest_fbo != nullptr) {
    dest_fbo->Unbind();
  }
}

uint32_t OITRenderQuad::vao_ = 0;

std::shared_ptr<Shader> OITRenderQuad::kShader = nullptr;
