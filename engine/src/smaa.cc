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

  Texture blend_texture(nullptr, width, height, GL_RGBA8, GL_RGB,
                        GL_UNSIGNED_BYTE, GL_CLAMP_TO_EDGE, GL_LINEAR,
                        GL_LINEAR, {}, false);
  std::vector<Texture> blend_textures;
  blend_textures.push_back(std::move(blend_texture));
  blend_fbo_.reset(new FrameBufferObject(blend_textures));
}

void SMAA::PrepareAreaAndSearchTexture(const std::string &smaa_repo_path) {
  area_ = Texture::LoadFromFS(smaa_repo_path + "/Textures/AreaTexDX10.dds",
                              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false,
                              false);
  search_ = Texture::LoadFromFS(smaa_repo_path + "/Textures/SearchTex.dds",
                                GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {},
                                false, false);
}

void SMAA::PrepareVertexData() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &ebo_);
  glGenBuffers(1, &vbo_);

  float vertices[] = {
      -1, -1, 0, 0, 0,  // bottom left corner
      -1, 3,  0, 0, 2,  // top left corner
      3,  -1, 0, 2, 0,  // bottom right corner
  };

  uint32_t indices[] = {0, 1, 2};

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
  PrepareAreaAndSearchTexture(smaa_repo_path);

  edge_detection_shader_.reset(new Shader(smaa_edge_detection_vs_source(),
                                          smaa_edge_detection_fs_source(), {}));
  blending_weight_calc_shader_.reset(
      new Shader(smaa_blending_weight_calc_vs_source(),
                 smaa_blending_weight_calc_fs_source(), {}));

  neighborhood_blending_shader_.reset(
      new Shader(smaa_neighborhood_blending_vs_source(),
                 smaa_neighborhood_blending_fs_source(), {}));
}

void SMAA::Draw() {
  glDisable(GL_BLEND);

  // run edge detection
  glViewport(0, 0, width_, height_);
  glBindVertexArray(vao_);
  edge_detection_shader_->Use();
  glm::vec4 rt_metrics(1.0 / width_, 1.0 / height_, width_, height_);
  edge_detection_shader_->SetUniform<glm::vec4>("SMAA_RT_METRICS", rt_metrics);
  edge_detection_shader_->SetUniformSampler("uColor",
                                            input_fbo_->color_texture(0), 0);
  edges_fbo_->Bind();
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
  edges_fbo_->Unbind();

  // run blending weight calc
  glViewport(0, 0, width_, height_);
  glBindVertexArray(vao_);
  blending_weight_calc_shader_->Use();
  blending_weight_calc_shader_->SetUniform<glm::vec4>("SMAA_RT_METRICS",
                                                      rt_metrics);
  blending_weight_calc_shader_->SetUniformSampler(
      "uEdges", edges_fbo_->color_texture(0), 0);
  blending_weight_calc_shader_->SetUniformSampler("uArea", area_, 1);
  blending_weight_calc_shader_->SetUniformSampler("uSearch", search_, 2);
  blend_fbo_->Bind();
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
  blend_fbo_->Unbind();

  // run neighborhood blending
  glViewport(0, 0, width_, height_);
  glBindVertexArray(vao_);
  neighborhood_blending_shader_->Use();
  neighborhood_blending_shader_->SetUniform<glm::vec4>("SMAA_RT_METRICS",
                                                       rt_metrics);
  neighborhood_blending_shader_->SetUniformSampler(
      "uColor", input_fbo_->color_texture(0), 0);
  neighborhood_blending_shader_->SetUniformSampler(
      "uBlend", blend_fbo_->color_texture(0), 1);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);

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

std::string SMAA::smaa_blending_weight_calc_vs_source() const {
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
out vec2 vPixCoord;

void main() {
    gl_Position = vec4(aPosition, 1);
    vTexCoord = aTexCoord;
    SMAABlendingWeightCalculationVS(vTexCoord, vPixCoord, vOffset);
}
)";
}

std::string SMAA::smaa_blending_weight_calc_fs_source() const {
  return std::string(R"(
#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
)") + kCommonDefines +
         smaa_lib_ + R"(
in vec4 vOffset[3];
in vec2 vTexCoord;
in vec2 vPixCoord;

uniform sampler2D uEdges;
uniform sampler2D uArea;
uniform sampler2D uSearch;

out vec4 fragColor;

void main() {
    fragColor = SMAABlendingWeightCalculationPS(vTexCoord, vPixCoord, vOffset, uEdges, uArea, uSearch, vec4(0));
    // gamma correction
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / 2.2));
}
)";
}

std::string SMAA::smaa_neighborhood_blending_vs_source() const {
  return std::string(R"(
#version 460 core

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
)") + kCommonDefines +
         smaa_lib_ + R"(
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec4 vOffset;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1);
    vTexCoord = aTexCoord;
    SMAANeighborhoodBlendingVS(vTexCoord, vOffset);
}
)";
}

std::string SMAA::smaa_neighborhood_blending_fs_source() const {
  return std::string(R"(
#version 460 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
)") + kCommonDefines +
         smaa_lib_ + R"(
in vec4 vOffset;
in vec2 vTexCoord;

uniform sampler2D uColor;
uniform sampler2D uBlend;

out vec4 fragColor;

void main() {
    fragColor = SMAANeighborhoodBlendingPS(vTexCoord, vOffset, uColor, uBlend);
}
)";
}