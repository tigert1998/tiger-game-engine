#include "light_sources.h"

#include <fmt/core.h>
#include <imgui.h>

#include "cg_exception.h"
#include "random/poisson_disk_generator.h"

AmbientLight::AmbientLight(glm::vec3 color) : color_(color) {}

void AmbientLight::ImGuiWindow(uint32_t index,
                               const std::function<void()> &erase_callback) {
  ImGui::Text("Ambient Light #%d:", index);
  std::string suffix = fmt::format("##ambient_light_{}", index);
  if (ImGui::Button(fmt::format("Erase{}", suffix).c_str())) {
    erase_callback();
    return;
  }

  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(fmt::format("Color{}", suffix).c_str(), c_arr);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
}

AmbientLight::AmbientLightGLSL AmbientLight::ambient_light_glsl() const {
  AmbientLightGLSL ret;
  ret.color = color_;
  return ret;
}

DirectionalLight::DirectionalLight(glm::vec3 dir, glm::vec3 color,
                                   Camera *camera)
    : dir_(dir), color_(color), camera_(camera) {}

void DirectionalLight::ImGuiWindow(
    uint32_t index, const std::function<void()> &erase_callback) {
  ImGui::Text("Directional Light #%d:", index);
  std::string suffix = fmt::format("##directional_light_{}", index);
  if (ImGui::Button(fmt::format("Erase{}", suffix).c_str())) {
    erase_callback();
    return;
  }

  float d_arr[3] = {dir_.x, dir_.y, dir_.z};
  ImGui::InputFloat3(fmt::format("Direction{}", suffix).c_str(), d_arr);
  dir_ = glm::vec3(d_arr[0], d_arr[1], d_arr[2]);
  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(fmt::format("Color{}", suffix).c_str(), c_arr);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);

  if (shadow_ == nullptr) {
    if (ImGui::Button(fmt::format("Add Shadow{}", suffix).c_str())) {
      shadow_ = std::unique_ptr<DirectionalShadow>(
          new DirectionalShadow(dir_, 2048, 2048, camera_));
    }
  } else {
    if (ImGui::Button(fmt::format("Erase Shadow{}", suffix).c_str())) {
      shadow_ = nullptr;
    }
  }

  if (shadow_ != nullptr) {
    shadow_->set_direction(dir_);
  }
}

DirectionalLight::DirectionalLightGLSL
DirectionalLight::directional_light_glsl() const {
  DirectionalLightGLSL ret;
  ret.dir = dir_;
  ret.color = color_;
  ret.shadow_enabled = shadow_ != nullptr;
  if (shadow_ != nullptr) {
    ret.shadow = shadow_->directional_shadow_glsl();
  }
  return ret;
}

PointLight::PointLight(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation)
    : pos_(pos), color_(color), attenuation_(attenuation) {}

void PointLight::ImGuiWindow(uint32_t index,
                             const std::function<void()> &erase_callback) {
  std::string suffix = fmt::format("##point_light_{}", index);

  ImGui::Text("Point Light #%d:", index);
  if (ImGui::Button(fmt::format("Erase{}", suffix).c_str())) {
    erase_callback();
    return;
  }

  float p_arr[3] = {pos_.x, pos_.y, pos_.z};
  ImGui::InputFloat3(fmt::format("Position{}", suffix).c_str(), p_arr);
  pos_ = glm::vec3(p_arr[0], p_arr[1], p_arr[2]);
  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(fmt::format("Color{}", suffix).c_str(), c_arr, 0);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
  float a_arr[3] = {attenuation_.x, attenuation_.y, attenuation_.z};
  ImGui::InputFloat3(fmt::format("Attenuation{}", suffix).c_str(), a_arr);
  attenuation_ = glm::vec3(a_arr[0], a_arr[1], a_arr[2]);

  if (shadow_ == nullptr) {
    if (ImGui::Button(fmt::format("Add Shadow{}", suffix).c_str())) {
      shadow_ = std::unique_ptr<OmnidirectionalShadow>(
          new OmnidirectionalShadow(pos_, 1, 2048, 2048));
    }
  } else {
    if (ImGui::Button(fmt::format("Erase Shadow{}", suffix).c_str())) {
      shadow_ = nullptr;
    }
  }

  if (shadow_ != nullptr) {
    shadow_->set_position(pos_);

    float radius = shadow_->radius();
    ImGui::SliderFloat(fmt::format("Shadow Radius{}", suffix).c_str(), &radius,
                       0, 10);
    shadow_->set_radius(radius);
  }
}

PointLight::PointLightGLSL PointLight::point_light_glsl() const {
  PointLightGLSL ret;
  ret.pos = pos_;
  ret.color = color_;
  ret.attenuation = attenuation_;
  ret.shadow_enabled = shadow_ != nullptr;
  if (shadow_ != nullptr) {
    ret.shadow = shadow_->omnidirectional_shadow_glsl();
  }
  return ret;
}

void LightSources::ResizeAmbientSSBO() {
  ambient_lights_ssbo_.reset(
      new SSBO(sizeof(AmbientLight::AmbientLightGLSL) * ambient_lights_.size(),
               nullptr, GL_DYNAMIC_DRAW, AmbientLight::GLSL_BINDING));
}

void LightSources::ResizeDirectioanlSSBO() {
  directional_lights_ssbo_.reset(
      new SSBO(sizeof(DirectionalLight::DirectionalLightGLSL) *
                   directional_lights_.size(),
               nullptr, GL_DYNAMIC_DRAW, DirectionalLight::GLSL_BINDING));
}

void LightSources::ResizePointSSBO() {
  point_lights_ssbo_.reset(
      new SSBO(sizeof(PointLight::PointLightGLSL) * point_lights_.size(),
               nullptr, GL_DYNAMIC_DRAW, PointLight::GLSL_BINDING));
}

LightSources::LightSources() {
  ResizeAmbientSSBO();
  ResizeDirectioanlSSBO();
  ResizePointSSBO();

  AllocatePoissonDiskSSBO();
}

void LightSources::AllocatePoissonDiskSSBO() {
  auto engine = std::default_random_engine{};

  auto generator = PoissonDiskGenerator();
  std::vector<glm::vec2> poisson_disk_2d_points = generator.Generate2D(128, 16);
  std::shuffle(poisson_disk_2d_points.begin(), poisson_disk_2d_points.end(),
               engine);
  poisson_disk_2d_points_ssbo_.reset(new SSBO(
      poisson_disk_2d_points, GL_STATIC_DRAW, POISSON_DISK_2D_BINDING));
}

uint32_t LightSources::SizeAmbient() const { return ambient_lights_.size(); }

uint32_t LightSources::SizeDirectional() const {
  return directional_lights_.size();
}

uint32_t LightSources::SizePoint() const { return point_lights_.size(); }

void LightSources::AddAmbient(std::unique_ptr<AmbientLight> light) {
  ambient_lights_.emplace_back(std::move(light));
  ResizeAmbientSSBO();
}

void LightSources::AddDirectional(std::unique_ptr<DirectionalLight> light) {
  directional_lights_.emplace_back(std::move(light));
  ResizeDirectioanlSSBO();
}

void LightSources::AddPoint(std::unique_ptr<PointLight> light) {
  point_lights_.emplace_back(std::move(light));
  ResizePointSSBO();
}

AmbientLight *LightSources::GetAmbient(uint32_t index) const {
  return ambient_lights_[index].get();
}

DirectionalLight *LightSources::GetDirectional(uint32_t index) const {
  return directional_lights_[index].get();
}

PointLight *LightSources::GetPoint(uint32_t index) const {
  return point_lights_[index].get();
}

void LightSources::ImGuiWindow(Camera *camera) {
  ImGui::Begin("Light Sources:");

  const char *light_source_types[] = {"Ambient", "Directional", "Point"};
  static int light_source_type = 0;
  ImGui::ListBox("##add_light_source", &light_source_type, light_source_types,
                 IM_ARRAYSIZE(light_source_types));
  ImGui::SameLine();
  if (ImGui::Button("Add##add_light_source")) {
    if (light_source_type == 0) {
      AddAmbient(std::make_unique<AmbientLight>(glm::vec3(0.1f)));
    } else if (light_source_type == 1) {
      AddDirectional(std::make_unique<DirectionalLight>(glm::vec3(0, -1, 0),
                                                        glm::vec3(1), camera));
    } else if (light_source_type == 2) {
      AddPoint(std::make_unique<PointLight>(glm::vec3(0), glm::vec3(1),
                                            glm::vec3(1, 0, 0)));
    }
  }

  for (int i = 0; i < ambient_lights_.size(); i++) {
    ambient_lights_[i]->ImGuiWindow(i, [this, i]() {
      this->ambient_lights_.erase(ambient_lights_.begin() + i);
      this->ResizeAmbientSSBO();
    });
  }

  for (int i = 0; i < directional_lights_.size(); i++) {
    directional_lights_[i]->ImGuiWindow(i, [this, i]() {
      this->directional_lights_.erase(directional_lights_.begin() + i);
      this->ResizeDirectioanlSSBO();
    });
  }

  for (int i = 0; i < point_lights_.size(); i++) {
    point_lights_[i]->ImGuiWindow(i, [this, i]() {
      this->point_lights_.erase(point_lights_.begin() + i);
      this->ResizePointSSBO();
    });
  }

  ImGui::End();
}

void LightSources::Set() {
  // ambient
  std::vector<AmbientLight::AmbientLightGLSL> ambient_light_glsl_vec;
  for (const auto &light : ambient_lights_) {
    ambient_light_glsl_vec.push_back(light->ambient_light_glsl());
  }
  glNamedBufferSubData(
      ambient_lights_ssbo_->id(), 0,
      ambient_light_glsl_vec.size() * sizeof(ambient_light_glsl_vec[0]),
      ambient_light_glsl_vec.data());
  ambient_lights_ssbo_->BindBufferBase();

  // directional
  std::vector<DirectionalLight::DirectionalLightGLSL>
      directional_light_glsl_vec;
  for (const auto &light : directional_lights_) {
    directional_light_glsl_vec.push_back(light->directional_light_glsl());
  }
  glNamedBufferSubData(
      directional_lights_ssbo_->id(), 0,
      directional_light_glsl_vec.size() * sizeof(directional_light_glsl_vec[0]),
      directional_light_glsl_vec.data());
  directional_lights_ssbo_->BindBufferBase();

  // point
  std::vector<PointLight::PointLightGLSL> point_light_glsl_vec;
  for (const auto &light : point_lights_) {
    point_light_glsl_vec.push_back(light->point_light_glsl());
  }
  glNamedBufferSubData(
      point_lights_ssbo_->id(), 0,
      point_light_glsl_vec.size() * sizeof(point_light_glsl_vec[0]),
      point_light_glsl_vec.data());
  point_lights_ssbo_->BindBufferBase();

  // poisson disk
  poisson_disk_2d_points_ssbo_->BindBufferBase();
}

void LightSources::DrawDepthForShadow(
    const std::function<void(int32_t, int32_t)> &render_pass) {
  glDisable(GL_CULL_FACE);
  for (int i = 0; i < directional_lights_.size(); i++) {
    if (directional_lights_[i]->shadow() == nullptr) continue;
    directional_lights_[i]->shadow()->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    render_pass(i, -1);
    directional_lights_[i]->shadow()->Unbind();
  }
  for (int i = 0; i < point_lights_.size(); i++) {
    if (point_lights_[i]->shadow() == nullptr) continue;
    point_lights_[i]->shadow()->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    render_pass(-1, i);
    point_lights_[i]->shadow()->Unbind();
  }
}