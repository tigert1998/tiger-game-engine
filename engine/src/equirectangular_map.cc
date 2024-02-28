#include "equirectangular_map.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "skybox.h"
#include "utils.h"

namespace fs = std::filesystem;

std::unique_ptr<Shader> EquirectangularMap::kConvertShader = nullptr;
std::unique_ptr<Shader> EquirectangularMap::kConvolutionShader = nullptr;

EquirectangularMap::EquirectangularMap(const std::filesystem::path &path,
                                       uint32_t width)
    : width_(width) {
  // create FBOs
  {
    Texture cubemap(std::vector<void *>{}, width, width, GL_RGB16F, GL_RGB,
                    GL_FLOAT, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {},
                    false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(cubemap));
    fbo_.reset(new FrameBufferObject(color_textures));
  }
  {
    Texture cubemap(std::vector<void *>{}, kConvolutionResolution,
                    kConvolutionResolution, GL_RGB16F, GL_RGB, GL_FLOAT,
                    GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(cubemap));
    convoluted_fbo_.reset(new FrameBufferObject(color_textures));
  }

  // compile the shader
  if (kConvertShader == nullptr && kConvolutionShader == nullptr) {
    kConvertShader.reset(
        new Shader("equirectangular_map/convert_to_cubemap.vert",
                   "equirectangular_map/convert_to_cubemap.frag", {}));
    kConvolutionShader.reset(
        new Shader("equirectangular_map/convert_to_cubemap.vert",
                   "equirectangular_map/convolution.frag", {}));
  }

  // load texture
  equirectangular_texture_ = Texture(path, GL_CLAMP_TO_EDGE, GL_LINEAR,
                                     GL_LINEAR, {}, false, true, false);

  // create vao
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  auto vertices = Skybox::vertices();
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(),
               vertices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  CHECK_OPENGL_ERROR();

  // convert to cubemap
  Draw();
}

void EquirectangularMap::Draw() {
  glm::mat4 projection_matrix =
      glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  glm::mat4 view_matrices[] = {
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f))};

  glBindVertexArray(vao_);

  // convert equirectangular texture to cubemap
  kConvertShader->Use();
  kConvertShader->SetUniform<glm::mat4>("uProjectionMatrix", projection_matrix);
  kConvertShader->SetUniformSampler("uTexture", equirectangular_texture_, 0);

  glViewport(0, 0, width_, width_);
  fbo_->Bind();
  for (int i = 0; i < 6; i++) {
    kConvertShader->SetUniform<glm::mat4>("uViewMatrix", view_matrices[i]);
    fbo_->SwitchAttachmentLayer(true, 0, i);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  fbo_->Unbind();

  // perform convolution to cubemap
  kConvolutionShader->Use();
  kConvolutionShader->SetUniform<glm::mat4>("uProjectionMatrix",
                                            projection_matrix);
  kConvolutionShader->SetUniformSampler("uTexture", cubemap(), 0);

  glViewport(0, 0, kConvolutionResolution, kConvolutionResolution);
  convoluted_fbo_->Bind();
  for (int i = 0; i < 6; i++) {
    kConvolutionShader->SetUniform<glm::mat4>("uViewMatrix", view_matrices[i]);
    convoluted_fbo_->SwitchAttachmentLayer(true, 0, i);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  fbo_->Unbind();

  glBindVertexArray(0);
}