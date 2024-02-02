#include "light_sources.h"

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
  if (ImGui::Button(("Erase##light_source_" + std::to_string(index)).c_str())) {
    erase_callback();
  }
  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(("Color##light_source_" + std::to_string(index)).c_str(),
                     c_arr);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
}

Directional::Directional(glm::vec3 dir, glm::vec3 color)
    : dir_(dir), color_(color) {}

void Directional::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uDirectionalLights.n") && not_found_ok)
    return;
  int32_t id = shader->GetUniform<int32_t>("uDirectionalLights.n");
  std::string var =
      std::string("uDirectionalLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".dir", dir_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<int32_t>("uDirectionalLights.n", id + 1);
}

void Directional::ImGuiWindow(uint32_t index,
                              const std::function<void()> &erase_callback) {
  ImGui::Text("Light Source #%d Type: Directional", index);
  if (ImGui::Button(("Erase##light_source_" + std::to_string(index)).c_str())) {
    erase_callback();
  }
  float d_arr[3] = {dir_.x, dir_.y, dir_.z};
  ImGui::InputFloat3(
      ("Direction##light_source_" + std::to_string(index)).c_str(), d_arr);
  dir_ = glm::vec3(d_arr[0], d_arr[1], d_arr[2]);
  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(("Color##light_source_" + std::to_string(index)).c_str(),
                     c_arr);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
}

Point::Point(glm::vec3 pos, glm::vec3 color, glm::vec3 attenuation)
    : pos_(pos), color_(color), attenuation_(attenuation) {}

void Point::Set(Shader *shader, bool not_found_ok) {
  if (!shader->UniformVariableExists("uPointLights.n") && not_found_ok) return;
  int32_t id = shader->GetUniform<int32_t>("uPointLights.n");
  std::string var =
      std::string("uPointLights.l") + "[" + std::to_string(id) + "]";
  shader->SetUniform<glm::vec3>(var + ".pos", pos_);
  shader->SetUniform<glm::vec3>(var + ".color", color_);
  shader->SetUniform<glm::vec3>(var + ".attenuation", attenuation_);
  shader->SetUniform<int32_t>("uPointLights.n", id + 1);
}

void Point::ImGuiWindow(uint32_t index,
                        const std::function<void()> &erase_callback) {
  ImGui::Text("Light Source #%d Type: Point", index);
  if (ImGui::Button(("Erase##light_source_" + std::to_string(index)).c_str())) {
    erase_callback();
  }
  float p_arr[3] = {pos_.x, pos_.y, pos_.z};
  ImGui::InputFloat3(
      ("Position##light_source_" + std::to_string(index)).c_str(), p_arr);
  pos_ = glm::vec3(p_arr[0], p_arr[1], p_arr[2]);
  float c_arr[3] = {color_.r, color_.g, color_.b};
  ImGui::InputFloat3(("Color##light_source_" + std::to_string(index)).c_str(),
                     c_arr, 0);
  color_ = glm::vec3(c_arr[0], c_arr[1], c_arr[2]);
  float a_arr[3] = {attenuation_.x, attenuation_.y, attenuation_.z};
  ImGui::InputFloat3(
      ("Attenuation##light_source_" + std::to_string(index)).c_str(), a_arr);
  attenuation_ = glm::vec3(a_arr[0], a_arr[1], a_arr[2]);
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

void LightSources::ImGuiWindow() {
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
      Add(std::make_unique<Directional>(glm::vec3(0, -1, 0), glm::vec3(1)));
    } else if (light_source_type == 2) {
      Add(std::make_unique<Point>(glm::vec3(0), glm::vec3(1),
                                  glm::vec3(1, 0, 0)));
    }
  }

  for (int i = 0; i < lights_.size(); i++) {
    lights_[i]->ImGuiWindow(
        i, [this, i]() { this->lights_.erase(lights_.begin() + i); });
  }
  ImGui::End();
}
