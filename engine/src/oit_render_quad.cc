#include "oit_render_quad.h"

#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

#include "texture_manager.h"
#include "utils.h"

void OITRenderQuad::Allocate(uint32_t width, uint32_t height,
                             float fragment_per_pixel) {
  uint32_t total_pixels = width * height;

  glGenTextures(1, &head_pointer_texture_);
  glBindTexture(GL_TEXTURE_2D, head_pointer_texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER,
               GL_UNSIGNED_INT, nullptr);

  glGenBuffers(1, &head_pointer_initializer_);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, head_pointer_initializer_);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, total_pixels * sizeof(uint32_t), nullptr,
               GL_STATIC_DRAW);
  uint32_t *data =
      (uint32_t *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  std::fill(data, data + total_pixels, 0xFFFFFFFF);
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  glGenBuffers(1, &atomic_counter_buffer_);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer_);
  glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr,
               GL_DYNAMIC_COPY);

  glGenBuffers(1, &fragment_storage_buffer_);
  glBindBuffer(GL_TEXTURE_BUFFER, fragment_storage_buffer_);
  glBufferData(
      GL_TEXTURE_BUFFER,
      (size_t)ceil(fragment_per_pixel * total_pixels) * sizeof(glm::uvec4),
      nullptr, GL_DYNAMIC_COPY);

  glGenTextures(1, &fragment_storage_texture_);

  fbo_.reset(new FrameBufferObject(width, height, true));
}

void OITRenderQuad::CopyDepthToDefaultFrameBuffer() {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_->id());
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                    GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void OITRenderQuad::Deallocate() {
  glDeleteTextures(1, &head_pointer_texture_);
  glDeleteBuffers(1, &head_pointer_initializer_);
  glDeleteBuffers(1, &atomic_counter_buffer_);
  glDeleteBuffers(1, &fragment_storage_buffer_);
  glDeleteTextures(1, &fragment_storage_texture_);
}

OITRenderQuad::OITRenderQuad(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
  Allocate(width, height, 16);

  if (kShader == nullptr) {
    kShader = ScreenSpaceShader(kFsSource);
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
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, head_pointer_initializer_);
  glBindTexture(GL_TEXTURE_2D, head_pointer_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width_, height_, 0, GL_RED_INTEGER,
               GL_UNSIGNED_INT, nullptr);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindImageTexture(0,  // unit
                     head_pointer_texture_, 0, false,
                     0,  // level, not layered, layer
                     GL_READ_WRITE, GL_R32UI);

  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_counter_buffer_);
  const uint32_t zero = 0;
  glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(zero), &zero);

  glBindTexture(GL_TEXTURE_BUFFER, fragment_storage_texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, fragment_storage_buffer_);
  glBindImageTexture(1, fragment_storage_texture_, 0, false, 0, GL_READ_WRITE,
                     GL_RGBA32UI);
}

void OITRenderQuad::Set(Shader *shader) {
  shader->SetUniform<int32_t>("uHeadPointers", 0);
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_counter_buffer_);
  // uListSize must have binding = 0
  shader->SetUniform<int32_t>("uList", 1);
}

void OITRenderQuad::Draw() {
  glDepthFunc(GL_ALWAYS);

  glBindVertexArray(vao_);
  shader_->Use();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, head_pointer_texture_);
  shader_->SetUniform<int32_t>("uHeadPointers", 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_BUFFER, fragment_storage_texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, fragment_storage_buffer_);
  shader_->SetUniform<int32_t>("uList", 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, fbo_->color_texture_id());
  shader_->SetUniform<int32_t>("uBackground", 2);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, fbo_->depth_texture_id());
  shader_->SetUniform<int32_t>("uBackgroundDepth", 3);

  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));

  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  glDepthFunc(GL_LESS);
}

uint32_t OITRenderQuad::vao_ = 0;

std::shared_ptr<Shader> OITRenderQuad::kShader = nullptr;

const std::string OITRenderQuad::kFsSource = R"(
#version 420 core

out vec4 fragColor;

uniform usampler2D uHeadPointers;
uniform usamplerBuffer uList;
uniform sampler2D uBackground;
uniform sampler2D uBackgroundDepth;
uniform vec2 uScreenSize;

const int MAX_FRAGMENTS = 128;
uvec4 fragments[MAX_FRAGMENTS];

int BuildLocalFragmentList() {
    uint current = texelFetch(uHeadPointers, ivec2(gl_FragCoord.xy), 0).r;
    int count = 0;
    while (current != 0xFFFFFFFF && count < MAX_FRAGMENTS) { 
        fragments[count] = texelFetch(uList, int(current)); 
        current = fragments[count].x;
        count++;
    }
    return count;
}

void SortFragmentList(int count) {
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            float depthI = uintBitsToFloat(fragments[i].z);
            float depthJ = uintBitsToFloat(fragments[j].z);
            // from far objects to near objects
            if (depthI < depthJ) {
                uvec4 temp = fragments[i];
                fragments[i] = fragments[j];
                fragments[j] = temp;
            }
        }
    }
}

vec4 CalcFragColor(int count, out float depth) {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec4 fragColor = texture(uBackground, coord);
    depth = texture(uBackgroundDepth, coord).r;
    for (int i = 0; i < count; i++) {
        vec4 color = unpackUnorm4x8(fragments[i].y);
        fragColor = mix(fragColor, color, color.a);
        depth = uintBitsToFloat(fragments[i].z);
    }
    return fragColor;
}

void main() {
    int count = BuildLocalFragmentList();
    SortFragmentList(count);
    float depth;
    fragColor = CalcFragColor(count, depth);
    gl_FragDepth = depth;
}
)";
