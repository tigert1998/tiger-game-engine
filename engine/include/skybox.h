#ifndef SKYBOX_H_
#define SKYBOX_H_

#include <memory>

#include "camera.h"
#include "cubemap_texture.h"
#include "shader.h"

class Skybox {
 private:
  CubemapTexture tex_;
  std::unique_ptr<Shader> shader_ptr_;
  uint32_t vao_, vbo_;

  static const std::string kVsSource;
  static const std::string kFsSource;
  static const float vertices_[36 * 3];

 public:
  Skybox(const std::string &path, const std::string &ext);
  void Draw(Camera *camera);
};

#endif