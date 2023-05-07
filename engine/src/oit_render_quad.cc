#include "oit_render_quad.h"

#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

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

  // generate framebuffer
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glGenTextures(1, &color_texture_);
  glBindTexture(GL_TEXTURE_2D, color_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_texture_, 0);
  glGenRenderbuffers(1, &depth_buffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, depth_buffer_);
  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OITRenderQuad::CopyDepthToDefaultFrameBuffer() {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
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

  glDeleteFramebuffers(1, &fbo_);
  glDeleteTextures(1, &color_texture_);
  glDeleteRenderbuffers(1, &depth_buffer_);
}

OITRenderQuad::OITRenderQuad(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
  Allocate(width, height, 16);

  if (kShader == nullptr) {
    kShader.reset(new Shader({{GL_VERTEX_SHADER, kVsSource},
                              {GL_GEOMETRY_SHADER, kGsSource},
                              {GL_FRAGMENT_SHADER, kFsSource}}));
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
  glBindTexture(GL_TEXTURE_2D, color_texture_);
  shader_->SetUniform<int32_t>("uBackground", 2);

  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));

  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
}

uint32_t OITRenderQuad::vao_ = 0;

std::shared_ptr<Shader> OITRenderQuad::kShader = nullptr;

const std::string OITRenderQuad::kVsSource = R"(
#version 410 core
void main() {}
)";

const std::string OITRenderQuad::kGsSource = R"(
#version 410 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

void main() {
    gl_Position = vec4(1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    EndPrimitive(); 
}
)";

const std::string OITRenderQuad::kFsSource = R"(
#version 420 core

out vec4 fragColor;

uniform usampler2D uHeadPointers;
uniform usamplerBuffer uList;
uniform sampler2D uBackground;
uniform vec2 uScreenSize;

const int MAX_FRAGMENTS = 64;
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

vec4 CalcFragColor(int count) {
    vec4 fragColor = texture(uBackground, gl_FragCoord.xy / uScreenSize);
    for (int i = 0; i < count; i++) {
        vec4 color = unpackUnorm4x8(fragments[i].y);
        fragColor = mix(fragColor, color, color.a);
    }
    return fragColor;
}

void main() {
    int count = BuildLocalFragmentList();
    SortFragmentList(count);
    fragColor = CalcFragColor(count);
}
)";
