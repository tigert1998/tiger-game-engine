#ifndef LIGHT_SOURCES_H_
#define LIGHT_SOURCES_H_

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "shader.h"

class Light {
 public:
  virtual void Set(Shader *shader) = 0;
  virtual ~Light(){};
};

class Ambient : public Light {
 private:
  glm::vec3 color_;

 public:
  Ambient(glm::vec3 color);
  void Set(Shader *shader) override;
  inline ~Ambient() override {}
};

class Directional : public Light {
 private:
  glm::vec3 dir_, color_;

 public:
  explicit Directional(glm::vec3 dir, glm::vec3 color);
  void Set(Shader *shader) override;
  inline ~Directional() override {}
};

class Point : public Light {
 private:
  glm::vec3 pos_, color_, attenuation_;

 public:
  explicit Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation);
  void Set(Shader *shader) override;
  inline ~Point() override {}
};

class LightSources : public Light {
 private:
  std::vector<std::unique_ptr<Light>> lights_;

 public:
  void Add(std::unique_ptr<Light> light);
  void Set(Shader *shader) override;
  inline ~LightSources() override {}

  static std::string kFsSource;
};

#endif