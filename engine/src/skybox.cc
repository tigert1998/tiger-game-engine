#include "skybox.h"

#include <glad/glad.h>

#include <glm/glm.hpp>

Skybox::Skybox(const std::string &path) {
  shader_ptr_ = std::unique_ptr<Shader>(
      new Shader("skybox/skybox.vert", "skybox/skybox.frag", {}));

  tex_ = Texture::LoadFromFS(path, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {},
                             false, false, true);

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);

  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);

  glBindVertexArray(0);
}

void Skybox::Draw(Camera *camera) {
  glDepthMask(GL_FALSE);

  shader_ptr_->Use();

  shader_ptr_->SetUniform<glm::mat4>(
      "uViewMatrix", glm::mat4(glm::mat3(camera->view_matrix())));
  shader_ptr_->SetUniform<glm::mat4>("uProjectionMatrix",
                                     camera->projection_matrix());
  shader_ptr_->SetUniformSampler("uSkyboxTexture", tex_, 0);

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