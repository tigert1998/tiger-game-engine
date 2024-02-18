#include "smaa.h"

#include <glad/glad.h>

#include "utils.h"

namespace fs = std::filesystem;

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

void SMAA::PrepareAreaAndSearchTexture(const fs::path &smaa_repo_path) {
  // sRGB can be any value since they are DDS images
  area_ = Texture::LoadFromFS(smaa_repo_path / "Textures/AreaTexDX10.dds",
                              GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false,
                              false, false);
  search_ = Texture::LoadFromFS(smaa_repo_path / "Textures/SearchTex.dds",
                                GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {},
                                false, false, false);
}

void SMAA::PrepareVertexData() {
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &ebo_);
  glGenBuffers(1, &vbo_);

  float vertices[] = {
      -1, -1, 0, 0, 0,  // bottom left corner
      3,  -1, 0, 2, 0,  // bottom right corner
      -1, 3,  0, 0, 2,  // top left corner
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

void SMAA::AddShaderIncludeDirectory(const fs::path &smaa_repo_path) {
  for (const auto &dir : Shader::include_directories) {
    if (dir == smaa_repo_path) return;
  }
  Shader::include_directories.push_back(smaa_repo_path);
}

SMAA::SMAA(const fs::path &smaa_repo_path, uint32_t width, uint32_t height) {
  Resize(width, height);
  PrepareVertexData();
  PrepareAreaAndSearchTexture(smaa_repo_path);
  AddShaderIncludeDirectory(smaa_repo_path);

  edge_detection_shader_.reset(
      new Shader("smaa/edge_detection.vert", "smaa/edge_detection.frag", {}));
  blending_weight_calc_shader_.reset(new Shader(
      "smaa/blending_weight_calc.vert", "smaa/blending_weight_calc.frag", {}));
  neighborhood_blending_shader_.reset(
      new Shader("smaa/neighborhood_blending.vert",
                 "smaa/neighborhood_blending.frag", {}));
}

void SMAA::Draw() {
  glDisable(GL_CULL_FACE);
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
