#define NOMINMAX

// clang-format off
#include <glad/glad.h>
// clang-format on

#include "shadows/shadow.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include "utils.h"

OmnidirectionalShadow::OmnidirectionalShadow(glm::vec3 position, float radius,
                                             uint32_t fbo_width,
                                             uint32_t fbo_height)
    : position_(position),
      radius_(radius),
      fbo_width_(fbo_width),
      fbo_height_(fbo_height) {
  std::vector<Texture> empty;
  Texture depth_texture(std::vector<void *>{}, fbo_width, fbo_height,
                        GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT,
                        GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR, {1, 1, 1, 1},
                        false);
  fbo_.reset(new FrameBufferObject(empty, depth_texture));
  fbo_->depth_texture().MakeResident();
}

std::vector<glm::mat4> OmnidirectionalShadow::view_projection_matrices() const {
  float aspect = (float)fbo_width_ / fbo_height_;
  glm::mat4 projection_matrix =
      glm::perspective(glm::radians(90.0f), aspect, z_near_, z_far_);

  std::vector<glm::mat4> ret;
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(1.0, 0.0, 0.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(-1.0, 0.0, 0.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 1.0, 0.0),
                            glm::vec3(0.0, 0.0, 1.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, -1.0, 0.0),
                            glm::vec3(0.0, 0.0, -1.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 0.0, 1.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  ret.push_back(projection_matrix *
                glm::lookAt(position_, position_ + glm::vec3(0.0, 0.0, -1.0),
                            glm::vec3(0.0, -1.0, 0.0)));
  return ret;
}

void OmnidirectionalShadow::Set(Shader *shader) {
  shader->SetUniform<glm::mat4>("uViewMatrix", glm::mat4(1));
  shader->SetUniform<glm::mat4>("uProjectionMatrix", glm::mat4(1));
}

void OmnidirectionalShadow::Bind() {
  glViewport(0, 0, fbo_width_, fbo_height_);
  fbo_->Bind();
}

void OmnidirectionalShadow::Clear() { glClear(GL_DEPTH_BUFFER_BIT); }

void OmnidirectionalShadow::Visualize() const {}

OmnidirectionalShadow::OmnidirectionalShadowGLSL
OmnidirectionalShadow::omnidirectional_shadow_glsl() const {
  OmnidirectionalShadowGLSL ret;
  auto vpms = view_projection_matrices();
  std::copy(vpms.begin(), vpms.end(), ret.view_projection_matrices);
  ret.shadow_map = fbo_->depth_texture().handle();
  ret.pos = position_;
  ret.radius = radius_;
  ret.far_plane = z_far_;
  return ret;
}

std::unique_ptr<Shader> DirectionalShadowViewer::kShader = nullptr;

DirectionalShadowViewer::DirectionalShadowViewer() {
  glGenVertexArrays(1, &vao_);
  if (kShader == nullptr) {
    kShader =
        Shader::ScreenSpaceShader("shadow/directional_shadow_viewer.frag", {});
  }
}

DirectionalShadowViewer::~DirectionalShadowViewer() {
  glDeleteVertexArrays(1, &vao_);
}

void DirectionalShadowViewer::Draw(glm::vec4 viewport,
                                   const Texture &shadow_map, uint32_t layer) {
  glBindVertexArray(vao_);

  kShader->Use();
  kShader->SetUniformSampler("uDirectionalShadowMap", shadow_map, 0);
  kShader->SetUniform<glm::vec4>("uViewport", viewport);
  kShader->SetUniform<uint32_t>("uLayer", layer);

  glViewport(viewport.x, viewport.y, viewport.z, viewport.w);
  glDrawArrays(GL_POINTS, 0, 1);

  glBindVertexArray(0);
}