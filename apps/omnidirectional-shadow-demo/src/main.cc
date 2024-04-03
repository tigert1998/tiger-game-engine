// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fmt/core.h>
#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <set>

#include "bloom.h"
#include "controller/sightseeing_controller.h"
#include "deferred_shading_render_quad.h"
#include "model.h"
#include "mouse.h"
#include "multi_draw_indirect.h"
#include "skybox.h"
#include "smaa.h"
#include "tone_mapping/aces.h"
#include "utils.h"

using namespace glm;
using namespace std;

class Controller : public SightseeingController {
 private:
  DeferredShadingRenderQuad *deferred_shading_render_quad_;
  PostProcesses *post_processes_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    deferred_shading_render_quad_->Resize(width, height);
    post_processes_->Resize(width, height);
  }

 public:
  Controller(Camera *camera,
             DeferredShadingRenderQuad *deferred_shading_render_quad,
             PostProcesses *post_processes, uint32_t width, uint32_t height,
             GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        deferred_shading_render_quad_(deferred_shading_render_quad),
        post_processes_(post_processes) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<MultiDrawIndirect> multi_draw_indirect;
std::unique_ptr<DeferredShadingRenderQuad> deferred_shading_render_quad_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Model> brick_wall_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Controller> controller_ptr;
std::unique_ptr<PostProcesses> post_processes_ptr;

struct PointLightWithSphere {
  PointLight *light;
  std::unique_ptr<MultiDrawIndirect> mdi;
  Model *model;
};
std::vector<PointLightWithSphere> point_lights_with_spheres;

int default_shading_choice = 0;
int enable_ssao = 0;
int enable_smaa = 0;
int enable_bloom = 0;

GLFWwindow *window;

void ImGuiInit() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = controller_ptr->width();
  io.DisplaySize.y = controller_ptr->height();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsClassic();
  auto font = io.Fonts->AddFontDefault();
  io.Fonts->Build();
}

PointLightWithSphere ConstructPointLightWithSphereModel(glm::vec3 emission) {
  static auto sphere = std::unique_ptr<Model>(
      new Model("resources/sphere/sphere_no_material.obj", false, false));

  light_sources_ptr->AddPoint(
      make_unique<PointLight>(glm::vec3(0), glm::vec3(0), glm::vec3(1, 0, 0)));
  auto light = light_sources_ptr->GetPoint(light_sources_ptr->SizePoint() - 1);
  auto material_params = sphere->mesh(0)->material_params();
  material_params->albedo = glm::vec3(0);
  material_params->metallic = 1;
  material_params->roughness = 0;
  material_params->emission = emission;

  auto mdi = std::unique_ptr<MultiDrawIndirect>(new MultiDrawIndirect());
  sphere->SubmitToMultiDrawIndirect(mdi.get(), 1);
  mdi->PrepareForDraw();
  return {light, std::move(mdi), sphere.get()};
}

void ImGuiWindow() {
  // model
  char buf[1 << 10] = {0};
  const char *choices[] = {"off", "on"};

  ImGui::Begin("Panel");
  ImGui::ListBox("Default shading", &default_shading_choice, choices,
                 IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SSAO", &enable_ssao, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SMAA", &enable_smaa, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable Bloom", &enable_bloom, choices, IM_ARRAYSIZE(choices));
  ImGui::End();

  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
  post_processes_ptr->ImGuiWindow();

  post_processes_ptr->Enable(0, enable_bloom);
  post_processes_ptr->Enable(2, enable_smaa);
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(
      width, height, "Multiple Omnidirectional Shadows Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  deferred_shading_render_quad_ptr.reset(
      new DeferredShadingRenderQuad(width, height));

  post_processes_ptr.reset(new PostProcesses());
  post_processes_ptr->Add(std::unique_ptr<Bloom>(new Bloom(width, height, 6)));
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::ACES>(
      new tone_mapping::ACES(width, height)));
  post_processes_ptr->Add(
      std::unique_ptr<SMAA>(new SMAA("./third_party/smaa", width, height)));

  camera_ptr = make_unique<Camera>(
      vec3(7, 9, 0), static_cast<double>(width) / height,
      -glm::pi<double>() / 2, 0, glm::radians(60.f), 0.1, 500);
  camera_ptr->set_front(-camera_ptr->position());

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->AddAmbient(make_unique<AmbientLight>(vec3(0.1)));

  model_ptr.reset(new Model("resources/dragon/dragon.obj", false, true));
  brick_wall_ptr.reset(
      new Model("resources/brickwall/brickwall.obj", false, false));
  multi_draw_indirect.reset(new MultiDrawIndirect());
  model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
  brick_wall_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
  multi_draw_indirect->PrepareForDraw();

  point_lights_with_spheres.push_back(
      ConstructPointLightWithSphereModel(glm::vec3(10, 0, 0)));
  point_lights_with_spheres.push_back(
      ConstructPointLightWithSphereModel(glm::vec3(0, 10, 0)));
  point_lights_with_spheres.push_back(
      ConstructPointLightWithSphereModel(glm::vec3(0, 0, 10)));

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), deferred_shading_render_quad_ptr.get(),
      post_processes_ptr.get(), width, height, window);

  ImGuiInit();
}

auto UpdateObjectsAndLights(double time) {
  std::vector<MultiDrawIndirect::RenderTargetParameter> render_target_params = {
      {model_ptr.get(),
       {{-1, 0,
         glm::scale(glm::translate(glm::mat4(1), glm::vec3(0, 2.75f, 0)),
                    glm::vec3(10.f)),
         glm::vec4(0)}}},
      {brick_wall_ptr.get(),
       {{-1, 0,
         glm::scale(glm::translate(glm::mat4(1), glm::vec3(-12.5f, 0, -12.5f)),
                    glm::vec3(25.f)),
         glm::vec4(0)}}}};

  std::vector<MultiDrawIndirect::RenderTargetParameter>
      render_target_params_lights;

  std::set<PointLight *> set;
  for (int i = 0; i < light_sources_ptr->SizePoint(); i++)
    set.insert(light_sources_ptr->GetPoint(i));

  std::vector<
      std::pair<MultiDrawIndirect *, MultiDrawIndirect::RenderTargetParameter>>
      params;
  if (auto light = point_lights_with_spheres[0].light; set.count(light)) {
    glm::vec3 position = glm::vec3(sin(time) * 10, 10, cos(time) * 10);
    MultiDrawIndirect::RenderTargetParameter param = {
        point_lights_with_spheres[0].model,
        {{-1, 0, glm::translate(glm::mat4(1), position)}},
    };
    light->set_color(glm::vec3(10, 0, 0));
    light->set_position(position);
    params.push_back({point_lights_with_spheres[0].mdi.get(), param});
  }

  if (auto light = point_lights_with_spheres[1].light; set.count(light)) {
    glm::vec3 position =
        glm::rotate(glm::mat4(1), glm::radians(30.f), glm::vec3(1, 0, 0)) *
        glm::vec4(sin(-time) * 10, 10, cos(-time) * 10, 1);
    MultiDrawIndirect::RenderTargetParameter param = {
        point_lights_with_spheres[1].model,
        {{-1, 0, glm::translate(glm::mat4(1), position)}},
    };
    light->set_color(glm::vec3(0, 10, 0));
    light->set_position(position);
    params.push_back({point_lights_with_spheres[1].mdi.get(), param});
  }

  if (auto light = point_lights_with_spheres[2].light; set.count(light)) {
    glm::vec3 position = glm::vec4(sin(time) * 5, cos(time) * 5 + 15, 0, 1);
    MultiDrawIndirect::RenderTargetParameter param = {
        point_lights_with_spheres[2].model,
        {{-1, 0, glm::translate(glm::mat4(1), position)}},
    };
    light->set_color(glm::vec3(0, 0, 10));
    light->set_position(position);
    params.push_back({point_lights_with_spheres[2].mdi.get(), param});
  }

  return std::pair{render_target_params, params};
}

void RenderLoop() {
  while (!glfwWindowShouldClose(window)) {
    static uint32_t fps = 0;
    static double last_time_for_fps = glfwGetTime();
    static double last_time = glfwGetTime();
    double current_time = glfwGetTime();
    double delta_time = current_time - last_time;
    last_time = current_time;

    Keyboard::shared.Elapse(delta_time);

    {
      fps += 1;
      if (current_time - last_time_for_fps >= 1.0) {
        char buf[1 << 10];
        sprintf(buf, "Multiple Omnidirectional Shadows Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    auto [render_target_params, render_target_params_lights] =
        UpdateObjectsAndLights(current_time);

    // draw depth map first
    light_sources_ptr->DrawDepthForShadow([&](int32_t directional_index,
                                              int32_t point_index) {
      multi_draw_indirect->DrawDepthForShadow(light_sources_ptr.get(),
                                              directional_index, point_index,
                                              render_target_params);
    });

    deferred_shading_render_quad_ptr->TwoPasses(
        camera_ptr.get(), light_sources_ptr.get(), enable_ssao, nullptr,
        []() {
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        },
        [&]() {
          glDisable(GL_CULL_FACE);
          multi_draw_indirect->Draw(camera_ptr.get(), nullptr, nullptr, true,
                                    nullptr, default_shading_choice, true,
                                    render_target_params);
          for (const auto &kv : render_target_params_lights) {
            kv.first->Draw(camera_ptr.get(), nullptr, nullptr, true, nullptr,
                           default_shading_choice, true, {kv.second});
          }
        },
        post_processes_ptr->fbo());

    post_processes_ptr->Draw(nullptr);

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuiWindow();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
}

int main(int argc, char *argv[]) {
  try {
    Init(1920, 1080);
    RenderLoop();
  } catch (const std::exception &e) {
    fmt::print(stderr, "[error] {}\n", e.what());
    exit(1);
  }

  return 0;
}