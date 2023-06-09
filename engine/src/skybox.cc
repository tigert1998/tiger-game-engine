#include "skybox.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

Skybox::Skybox(const std::string &path, const std::string &ext)
    : tex_(path, ext) {
  shader_ptr_ =
      std::unique_ptr<Shader>(new Shader(Skybox::kVsSource, Skybox::kFsSource));

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
  tex_.Bind(GL_TEXTURE0);
  shader_ptr_->SetUniform("uSkyboxTexture", 0);

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
}

const std::string Skybox::kVsSource = R"(
#version 410 core
layout (location = 0) in vec3 aPosition;

out vec3 vTexCoord;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;

void main() {
    vTexCoord = aPosition;
    gl_Position = uProjectionMatrix * uViewMatrix * vec4(aPosition, 1);
}
)";
const std::string Skybox::kFsSource = R"(
#version 410 core
out vec4 fragColor;

in vec3 vTexCoord;

uniform samplerCube uSkyboxTexture;

void main() {
    fragColor = texture(uSkyboxTexture, vTexCoord);
}
)";

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