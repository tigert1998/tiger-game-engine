#include "light_sources.h"

#include <fmt/core.h>
#include <imgui.h>

#include "cg_exception.h"

Ambient::Ambient(glm::vec3 color) : color_(color) {}

void Ambient::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uAmbientLights.n") && not_found_ok)
    return;
  int32_t id = shader->GetUniform<int32_t>("uAmbientLights.n");
  std::string var =
      std::string("uAmbientLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uAmbientLights.n", id + 1);
}

void Ambient::ImGuiWindow(uint32_t index,
                          const std::function<void()> &erase_callback) {
  ImGui::Text("Light Source #%d Type: Ambient", index);
  if (ImGui::Button(fmt::format("Erase##light_source_{}", index).c_str())) {
    erase_callback();
    return;
  }

  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(fmt::format("Color##light_source_{}", index).c_str(),
                     c_arr);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
}

Directional::Directional(glm::vec3 dir, glm::vec3 color, Camera *camera,
                         ShadowSources *shadow_sources)
    : dir_(dir),
      color_(color),
      camera_(camera),
      shadow_sources_(shadow_sources) {}

void Directional::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uDirectionalLights.n") && not_found_ok)
    return;
  int32_t id = shader->GetUniform<int32_t>("uDirectionalLights.n");
  std::string var =
      std::string("uDirectionalLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".dir", dir_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  int32_t shadowIndex =
      shadow_ == nullptr ? -1 : shadow_sources_->GetDirectionalIndex(shadow_);
  shader->SetUniform<int32_t>(var + ".shadowIndex", shadowIndex);
  shader->SetUniform<int32_t>("uDirectionalLights.n", id + 1);
}

void Directional::ImGuiWindow(uint32_t index,
                              const std::function<void()> &erase_callback) {
  std::string suffix = fmt::format("##light_source_{}", index);
  ImGui::Text("Light Source #%d Type: Directional", index);
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
      auto ptr = std::unique_ptr<DirectionalShadow>(
          new DirectionalShadow(dir_, 2048, 2048, camera_));
      shadow_ = ptr.get();
      shadow_sources_->AddDirectional(std::move(ptr));
    }
  } else {
    if (ImGui::Button(fmt::format("Erase Shadow{}", suffix).c_str())) {
      shadow_sources_->EraseDirectional(shadow_);
      shadow_ = nullptr;
    }
  }

  if (shadow_ != nullptr) {
    shadow_->set_direction(dir_);
  }
}

Point::Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation,
             ShadowSources *shadow_sources)
    : pos_(pos),
      color_(color),
      attenuation_(attenuation),
      shadow_sources_(shadow_sources) {}

void Point::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uPointLights.n") && not_found_ok) return;
  int32_t id = shader->GetUniform<int32_t>("uPointLights.n");
  std::string var =
      std::string("uPointLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".pos", pos_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<glm::vec3>(var + ".attenuation", attenuation_);
  int32_t shadowIndex = shadow_ == nullptr
                            ? -1
                            : shadow_sources_->GetOmnidirectionalIndex(shadow_);
  shader->SetUniform<int32_t>(var + ".shadowIndex", shadowIndex);
  shader->SetUniform<int32_t>("uPointLights.n", id + 1);
}

void Point::ImGuiWindow(uint32_t index,
                        const std::function<void()> &erase_callback) {
  std::string suffix = fmt::format("##light_source_{}", index);

  ImGui::Text("Light Source #%d Type: Point", index);
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
      auto ptr = std::unique_ptr<OmnidirectionalShadow>(
          new OmnidirectionalShadow(pos_, 2048, 2048));
      shadow_ = ptr.get();
      shadow_sources_->AddOmnidirectional(std::move(ptr));
    }
  } else {
    if (ImGui::Button(fmt::format("Erase Shadow{}", suffix).c_str())) {
      shadow_sources_->EraseOmnidirectional(shadow_);
      shadow_ = nullptr;
    }
  }

  if (shadow_ != nullptr) {
    shadow_->set_position(pos_);
  }
}

void LightSources::Add(std::unique_ptr<Light> light) {
  lights_.emplace_back(std::move(light));
}

Light *LightSources::Get(int32_t index) { return lights_[index].get(); }

void LightSources::Set(Shader *shader, bool not_found_ok) {
  for (auto name :
       std::vector<std::string>{"Ambient", "Directional", "Point"}) {
    std::string var = std::string("u") + name + "Lights.n";
    if (!shader->UniformVariableExists(var) && not_found_ok) continue;
    shader->SetUniform<int32_t>(var, 0);
  }

  for (int i = 0; i < lights_.size(); i++) {
    lights_[i]->Set(shader, not_found_ok);
  }
}

void LightSources::ImGuiWindow(Camera *camera, ShadowSources *shadow_sources) {
  ImGui::Begin("Light Sources:");

  const char *light_source_types[] = {"Ambient", "Directional", "Point"};
  static int light_source_type = 0;
  ImGui::ListBox("##add_light_source", &light_source_type, light_source_types,
                 IM_ARRAYSIZE(light_source_types));
  ImGui::SameLine();
  if (ImGui::Button("Add##add_light_source")) {
    if (light_source_type == 0) {
      Add(std::make_unique<Ambient>(glm::vec3(0.1f)));
    } else if (light_source_type == 1) {
      Add(std::make_unique<Directional>(glm::vec3(0, -1, 0), glm::vec3(1),
                                        camera, shadow_sources));
    } else if (light_source_type == 2) {
      Add(std::make_unique<Point>(glm::vec3(0), glm::vec3(1),
                                  glm::vec3(1, 0, 0), shadow_sources));
    }
  }

  for (int i = 0; i < lights_.size(); i++) {
    lights_[i]->ImGuiWindow(
        i, [this, i]() { this->lights_.erase(lights_.begin() + i); });
  }
  ImGui::End();
}
