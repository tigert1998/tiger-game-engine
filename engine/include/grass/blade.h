#ifndef GRASS_BLADE_H_
#define GRASS_BLADE_H_

#include <memory>
#include <string>

#include "shader.h"

class Blade {
 private:
  const static std::string kOBJSource;

  uint32_t vao_, vbo_, ebo_, indices_size_;

  std::shared_ptr<Shader> shader_;

 public:
  Blade();

  inline uint32_t vao() const { return vao_; }

  inline uint32_t indices_size() const { return indices_size_; }

  inline Shader* shader() { return shader_.get(); }
};

#endif