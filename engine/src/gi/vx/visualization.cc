#include "gi/vx/visualization.h"

#include <glad/glad.h>

#include "skybox.h"
#include "utils.h"

namespace vxgi {

std::unique_ptr<Shader> Visualization::kCubeShader = nullptr;
std::unique_ptr<Shader> Visualization::kVisualizationShader = nullptr;

Visualization::Visualization(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
  if (kCubeShader == nullptr && kVisualizationShader == nullptr) {
    kCubeShader.reset(new Shader("vxgi/cube.vert", "vxgi/cube.frag", {}));
    kVisualizationShader = Shader::ScreenSpaceShader("vxgi/visualize.frag", {});
  }

  // create vao and vbo
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

  {
    Texture texture(nullptr, width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                    GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(texture));
    front_fbo_.reset(new FrameBufferObject(color_textures));
  }
  {
    Texture texture(nullptr, width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                    GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(texture));
    back_fbo_.reset(new FrameBufferObject(color_textures));
  }
}

Visualization::~Visualization() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
}

void Visualization::DrawCubeCoord(const Camera* camera,
                                  const FrameBufferObject* fbo,
                                  uint32_t cull_face, float world_size) {
  glViewport(0, 0, width_, height_);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glCullFace(cull_face);

  fbo->Bind();
  kCubeShader->Use();
  kCubeShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  kCubeShader->SetUniform<glm::mat4>("uProjectionMatrix",
                                     camera->projection_matrix());
  kCubeShader->SetUniform<float>("uWorldSize", world_size);
  glBindVertexArray(vao_);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
  fbo->Unbind();

  glEnable(GL_DEPTH_TEST);
}

void Visualization::Draw(const Camera* camera, const Texture& voxel,
                         uint32_t voxel_type, float world_size,
                         uint32_t mipmap_level) {
  DrawCubeCoord(camera, front_fbo_.get(), GL_BACK, world_size);
  DrawCubeCoord(camera, back_fbo_.get(), GL_FRONT, world_size);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  kVisualizationShader->Use();
  kVisualizationShader->SetUniform<glm::vec3>("uCameraPosition",
                                              camera->position());
  kVisualizationShader->SetUniform<uint32_t>("uVoxelType", voxel_type);
  if (voxel_type == 0) {
    kVisualizationShader->SetUniformSampler("uVoxelU", voxel, 0);
    kVisualizationShader->SetUniformSampler("uVoxelF",
                                            Texture::Empty(GL_TEXTURE_3D), 1);
  } else if (voxel_type == 1) {
    kVisualizationShader->SetUniformSampler("uVoxelU",
                                            Texture::Empty(GL_TEXTURE_3D), 0);
    kVisualizationShader->SetUniformSampler("uVoxelF", voxel, 1);
  }
  kVisualizationShader->SetUniformSampler("uFront",
                                          front_fbo_->color_texture(0), 2);
  kVisualizationShader->SetUniformSampler("uBack", back_fbo_->color_texture(0),
                                          3);
  kVisualizationShader->SetUniform<uint32_t>("uMipmapLevel", mipmap_level);
  kVisualizationShader->SetUniform<glm::vec4>("uViewport",
                                              glm::vec4(0, 0, width_, height_));
  kVisualizationShader->SetUniform<float>("uWorldSize", world_size);

  glViewport(0, 0, width_, height_);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glBindVertexArray(vao_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);

  glDisable(GL_CULL_FACE);
}

}  // namespace vxgi