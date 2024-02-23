#ifndef LIGHT_SOURCES_H_
#define LIGHT_SOURCES_H_

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "shader.h"
#include "shadows.h"
#include "ssbo.h"

class Light {
 public:
  virtual void ImGuiWindow(uint32_t index,
                           const std::function<void()> &erase_callback) = 0;
  virtual ~Light(){};
};

class AmbientLight : public Light {
 private:
  glm::vec3 color_;

 public:
  static constexpr uint32_t GLSL_BINDING = 16;

  AmbientLight(glm::vec3 color);
  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~AmbientLight() override {}

  struct AmbientLightGLSL {
    alignas(16) glm::vec3 color;
  };
  AmbientLightGLSL ambient_light_glsl() const;
};

class DirectionalLight : public Light {
 private:
  glm::vec3 dir_, color_;
  std::unique_ptr<DirectionalShadow> shadow_ = nullptr;
  Camera *camera_;

 public:
  static constexpr uint32_t GLSL_BINDING = 17;

  explicit DirectionalLight(glm::vec3 dir, glm::vec3 color, Camera *camera);
  inline glm::vec3 dir() { return dir_; }
  inline glm::vec3 color() { return color_; }
  inline void set_dir(glm::vec3 dir) { dir_ = dir; }
  inline void set_color(glm::vec3 color) { color_ = color; }
  inline DirectionalShadow *shadow() { return shadow_.get(); }
  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~DirectionalLight() override {}

  struct DirectionalLightGLSL {
    alignas(16) glm::vec3 dir;
    alignas(16) glm::vec3 color;
    alignas(4) int32_t shadow_enabled;
    DirectionalShadow::DirectionalShadowGLSL shadow;
  };
  DirectionalLightGLSL directional_light_glsl() const;
};

class PointLight : public Light {
 private:
  glm::vec3 pos_, color_, attenuation_;
  std::unique_ptr<OmnidirectionalShadow> shadow_ = nullptr;

 public:
  static constexpr uint32_t GLSL_BINDING = 18;

  explicit PointLight(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation);

  inline glm::vec3 position() { return pos_; }
  inline glm::vec3 color() { return color_; }
  inline glm::vec3 attenuation() { return attenuation_; }
  inline void set_position(glm::vec3 pos) { pos_ = pos; }
  inline void set_color(glm::vec3 color) { color_ = color; }
  inline void set_attenuation(glm::vec3 attenuation) {
    attenuation_ = attenuation;
  }

  inline OmnidirectionalShadow *shadow() { return shadow_.get(); }

  void ImGuiWindow(uint32_t index,
                   const std::function<void()> &erase_callback) override;
  inline ~PointLight() override {}

  struct PointLightGLSL {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 attenuation;
    alignas(4) int32_t shadow_enabled;
    OmnidirectionalShadow::OmnidirectionalShadowGLSL shadow;
  };
  PointLightGLSL point_light_glsl() const;
};

class LightSources {
 private:
  std::unique_ptr<SSBO> ambient_lights_ssbo_;
  std::unique_ptr<SSBO> directional_lights_ssbo_;
  std::unique_ptr<SSBO> point_lights_ssbo_;

  void ResizeAmbientSSBO();
  void ResizeDirectioanlSSBO();
  void ResizePointSSBO();

  std::vector<std::unique_ptr<AmbientLight>> ambient_lights_;
  std::vector<std::unique_ptr<DirectionalLight>> directional_lights_;
  std::vector<std::unique_ptr<PointLight>> point_lights_;

 public:
  explicit LightSources();

  uint32_t SizeAmbient() const;
  uint32_t SizeDirectional() const;
  uint32_t SizePoint() const;
  void AddAmbient(std::unique_ptr<AmbientLight> light);
  void AddDirectional(std::unique_ptr<DirectionalLight> light);
  void AddPoint(std::unique_ptr<PointLight> light);
  AmbientLight *GetAmbient(uint32_t index) const;
  DirectionalLight *GetDirectional(uint32_t index) const;
  PointLight *GetPoint(uint32_t index) const;

  void Set();
  void DrawDepthForShadow(
      const std::function<void(int32_t, int32_t)> &render_pass);

  void ImGuiWindow(Camera *camera);
};

#endif