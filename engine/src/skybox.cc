#include "skybox.h"

#include <glad/glad.h>

#include <glm/glm.hpp>

namespace fs = std::filesystem;

void Skybox::Init() {
  shader_ptr_ = std::unique_ptr<Shader>(
      new Shader("skybox/skybox.vert", "skybox/skybox.frag", {}));

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Skybox::Skybox(const fs::path &path, bool tone_map) : tone_map_(tone_map) {
  texture_ = Texture(path, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false,
                     false, true);
  Init();
}

Skybox::Skybox(const Texture *outside_texture, bool tone_map)
    : outside_texture_(outside_texture), tone_map_(tone_map) {
  Init();
}

void Skybox::Draw(Camera *camera) {
  glDepthMask(GL_FALSE);

  shader_ptr_->Use();

  shader_ptr_->SetUniform<glm::mat4>(
      "uViewMatrix", glm::mat4(glm::mat3(camera->view_matrix())));
  shader_ptr_->SetUniform<glm::mat4>("uProjectionMatrix",
                                     camera->projection_matrix());
  shader_ptr_->SetUniformSampler(
      "uSkyboxTexture",
      outside_texture_ != nullptr ? *outside_texture_ : texture_, 0);
  shader_ptr_->SetUniform<glm::int32_t>("uToneMap", tone_map_);

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
}

const float Skybox::vertices_[36 * 3] = {
    // positions
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};