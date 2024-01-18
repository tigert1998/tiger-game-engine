#include "smaa.h"

#include <glad/glad.h>

#include "utils.h"

void SMAA::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;

  std::vector<Texture> color_textures;
  color_textures.push_back(Texture(nullptr, width, height, GL_RGBA, GL_RGBA,
                                   GL_UNSIGNED_BYTE, GL_CLAMP_TO_EDGE,
                                   GL_LINEAR, GL_LINEAR, {}, false));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  input_fbo_.reset(new FrameBufferObject(color_textures, depth_texture));

  Texture edges_texture(nullptr, width, height, GL_RGBA8, GL_RGB,
                        GL_UNSIGNED_BYTE, GL_CLAMP_TO_EDGE, GL_LINEAR,
                        GL_LINEAR, {}, false);
  std::vector<Texture> edges_textures;
  edges_textures.push_back(std::move(edges_texture));
  edges_fbo_.reset(new FrameBufferObject(edges_textures));
}

void SMAA::PrepareVertexData() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &ebo_);
  glGenBuffers(1, &vbo_);

  float vertices[] = {-1, -1, 0, 0, 0,   // bottom left corner
                      -1, 1,  0, 0, 1,   // top left corner
                      1,  1,  0, 1, 1,   // top right corner
                      1,  -1, 0, 1, 0};  // bottom right corner

  uint32_t indices[] = {
      0, 1, 2,   // first triangle (bottom left - top left - top right)
      0, 2, 3};  // second triangle (bottom left - top right - bottom right)

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(vertices[0]),
                        (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(vertices[0]),
                        (void *)(3 * sizeof(vertices[0])));

  glBindVertexArray(0);
}

SMAA::~SMAA() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &ebo_);
  glDeleteBuffers(1, &vbo_);
}

SMAA::SMAA(const std::string &smaa_repo_path, uint32_t width, uint32_t height) {
  smaa_lib_ = ReadFile(smaa_repo_path + "/SMAA.hlsl");

  Resize(width, height);
  PrepareVertexData();

  // CHECK_OPENGL_ERROR();

  edge_detection_shader_.reset(new Shader(smaa_edge_detection_vs_source(),
                                          smaa_edge_detection_fs_source(), {}));
}

void SMAA::Draw() {
  glDisable(GL_BLEND);

  // run edge detection
  glViewport(0, 0, width_, height_);
  glBindVertexArray(vao_);
  edge_detection_shader_->Use();
  edge_detection_shader_->SetUniform<glm::vec4>(
      "SMAA_RT_METRICS",
      glm::vec4(1.0 / width_, 1.0 / height_, width_, height_));
  edge_detection_shader_->SetUniformSampler("uColor",
                                            input_fbo_->color_texture(0), 0);
  edges_fbo_->Bind();
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  edges_fbo_->Unbind();

  glEnable(GL_BLEND);
}

const std::string SMAA::kCommonDefines = R"(
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
uniform vec4 SMAA_RT_METRICS;
)";

std::string SMAA::smaa_edge_detection_vs_source() const {
  return std::string(R"(
#version 460 core

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
)") + kCommonDefines +
         smaa_lib_ + R"(
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec4 vOffset[3];
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1);
    vTexCoord = aTexCoord;
    SMAAEdgeDetectionVS(vTexCoord, vOffset);
}
)";
}

std::string SMAA::smaa_edge_detection_fs_source() const {
  return std::string(R"(
#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
)") + kCommonDefines +
         smaa_lib_ + R"(
uniform sampler2D uColor;

in vec4 vOffset[3];
in vec2 vTexCoord;

out vec4 fragColor;

void main() {
    fragColor = vec4(SMAALumaEdgeDetectionPS(vTexCoord, vOffset, uColor), vec2(0));
}
)";
}