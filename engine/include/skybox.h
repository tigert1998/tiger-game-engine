#ifndef SKYBOX_H_
#define SKYBOX_H_

#include <memory>

#include "camera.h"
#include "shader.h"
#include "texture.h"

class Skybox {
 private:
  Texture tex_;
  std::unique_ptr<Shader> shader_ptr_;
  uint32_t vao_, vbo_;

  static const float vertices_[36 * 3];

 public:
  Skybox(const std::string &path);
  void Draw(Camera *camera);
};

#endif