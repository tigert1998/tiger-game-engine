#ifndef LIGHT_SOURCES_H_
#define LIGHT_SOURCES_H_

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "shader.h"
#include "shadow_sources.h"

class Light {
 public:
  virtual void Set(Shader *shader, bool not_found_ok) = 0;
  virtual void ImGuiWindow(uint32_t index,
                           const std::function<void()> &erase_callback) = 0;
  virtual ~Light(){};
};

class Ambient : public Light {
 private:
  glm::vec3 color_;

 public:
  Ambient(glm::vec3 color);
  void Set(Shader *shader, bool not_found_ok) override;
  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~Ambient() override {}
};

class Directional : public Light {
 private:
  glm::vec3 dir_, color_;
  DirectionalShadow *shadow_ = nullptr;
  ShadowSources *shadow_sources_;
  Camera *camera_;

 public:
  explicit Directional(glm::vec3 dir, glm::vec3 color, Camera *camera,
                       ShadowSources *shadow_sources);
  inline glm::vec3 dir() { return dir_; }
  inline glm::vec3 color() { return color_; }
  inline void set_dir(glm::vec3 dir) { dir_ = dir; }
  inline void set_color(glm::vec3 color) { color_ = color; }
  void Set(Shader *shader, bool not_found_ok) override;
  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~Directional() override {}
};

class Point : public Light {
 private:
  glm::vec3 pos_, color_, attenuation_;
  OmnidirectionalShadow *shadow_ = nullptr;
  ShadowSources *shadow_sources_;

 public:
  explicit Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation,
                 ShadowSources *shadow_sources);
  void Set(Shader *shader, bool not_found_ok) override;
  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~Point() override {}
};

class LightSources {
 private:
  std::vector<std::unique_ptr<Light>> lights_;

 public:
  void Add(std::unique_ptr<Light> light);
  Light *Get(int32_t index);
  void Set(Shader *shader, bool not_found_ok);

  void ImGuiWindow(Camera *camera, ShadowSources *shadow_sources);
};

#endif