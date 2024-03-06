#ifndef SKYBOX_H_
#define SKYBOX_H_

#include <filesystem>
#include <memory>
#include <vector>

#include "camera.h"
#include "shader.h"
#include "texture.h"

class Skybox {
 private:
  const Texture *outside_texture_ = nullptr;
  Texture texture_;
  std::unique_ptr<Shader> shader_ptr_;
  uint32_t vao_, vbo_;

  static const float vertices_[36 * 3];

  void Init();

 public:
  Skybox(const std::filesystem::path &path);
  Skybox(const Texture *outside_texture);
  void Draw(Camera *camera);
  inline static std::vector<float> vertices() {
    return std::vector<float>(std::begin(vertices_), std::end(vertices_));
  }
};

#endif