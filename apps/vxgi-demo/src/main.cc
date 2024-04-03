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

#include "bloom.h"
#include "controller/sightseeing_controller.h"
#include "deferred_shading_render_quad.h"
#include "equirectangular_map.h"
#include "gi/vx/light_injection.h"
#include "gi/vx/visualization.h"
#include "gi/vx/voxelization.h"
#include "model.h"
#include "mouse.h"
#include "multi_draw_indirect.h"
#include "post_processes.h"
#include "smaa.h"
#include "tone_mapping/bilateral_grid.h"
#include "utils.h"

using namespace glm;
using namespace std;
namespace fs = std::filesystem;

class Controller : public SightseeingController {
 private:
  const std::function<void(uint32_t, uint32_t)> &resize_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    resize_(width, height);
  }

 public:
  using ResizeFunc = std::function<void(uint32_t, uint32_t)>;

  Controller(Camera *camera, const ResizeFunc &resize, uint32_t width,
             uint32_t height, GLFWwindow *window)
      : SightseeingController(camera, width, height, window), resize_(resize) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<MultiDrawIndirect> multi_draw_indirect;
std::unique_ptr<DeferredShadingRenderQuad> deferred_shading_render_quad_ptr;
std::unique_ptr<PostProcesses> post_processes_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<EquirectangularMap> equirectangular_map_ptr;
std::unique_ptr<Controller> controller_ptr;

std::unique_ptr<vxgi::Voxelization> voxelization_ptr;
std::unique_ptr<vxgi::Visualization> voxelization_visualization_ptr;
std::unique_ptr<vxgi::LightInjection> injection_ptr;
std::unique_ptr<vxgi::VXGIConfig> vxgi_config_ptr;

int default_shading_choice = 0;
int enable_ssao = 0;
int enable_smaa = 0;
int enable_bloom = 0;
int enable_voxelization_visualization = 0;
int mipmap_level = 0;
bool revoxelization = false;

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

void ReloadModel(const fs::path &path, float voxel_world_size,
                 uint32_t voxel_resolution) {
  model_ptr.reset(new Model(path, true, true));
  multi_draw_indirect.reset(new MultiDrawIndirect());
  model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(), 1);
  multi_draw_indirect->PrepareForDraw();

  voxelization_ptr.reset(
      new vxgi::Voxelization(voxel_world_size, voxel_resolution));
  injection_ptr.reset(
      new vxgi::LightInjection(voxel_world_size, voxel_resolution));
  vxgi_config_ptr.reset(new vxgi::VXGIConfig(voxel_world_size, voxel_resolution,
                                             injection_ptr->light_injected()));

  revoxelization = true;
}

void ImGuiWindow() {
  // model
  static char buf[1 << 10] = {0};
  const char *choices[] = {"off", "on"};
  static float world_size = 20;
  static int32_t resolution = 256;

  ImGui::Begin("Panel");
  ImGui::InputFloat("World Size", &world_size);
  ImGui::InputInt("Voxelization Resolution", &resolution);
  if (ImGui::InputText("Model Path", buf, sizeof(buf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    ReloadModel(buf, world_size, resolution);
  }
  ImGui::ListBox("Default Shading", &default_shading_choice, choices,
                 IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SSAO", &enable_ssao, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable SMAA", &enable_smaa, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable Bloom", &enable_bloom, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("Enable Voxelization Visualization",
                 &enable_voxelization_visualization, choices,
                 IM_ARRAYSIZE(choices));
  if (enable_voxelization_visualization) {
    ImGui::InputInt("Mipmap Level", &mipmap_level);
  }
  ImGui::End();

  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
  post_processes_ptr->ImGuiWindow();
  vxgi_config_ptr->ImGuiWindow();

  post_processes_ptr->Enable(0, enable_bloom);
  post_processes_ptr->Enable(2, enable_smaa);
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "VXGI Demo", nullptr, nullptr);
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
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::BilateralGrid>(
      new tone_mapping::BilateralGrid(width, height)));
  post_processes_ptr->Add(
      std::unique_ptr<SMAA>(new SMAA("./third_party/smaa", width, height)));

  camera_ptr = make_unique<Camera>(
      vec3(7, 9, 0), static_cast<double>(width) / height,
      -glm::pi<double>() / 2, 0, glm::radians(60.f), 0.1, 100);
  camera_ptr->set_front(-camera_ptr->position());

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->AddDirectional(make_unique<DirectionalLight>(
      vec3(0, -1, 0), vec3(50), camera_ptr.get()));
  light_sources_ptr->GetDirectional(0)->set_shadow(
      std::unique_ptr<DirectionalShadow>(new DirectionalShadow(
          vec3(0, -1, 0), 2048, 2048, AABB(glm::vec3(-20), glm::vec3(20)),
          camera_ptr.get())));

  equirectangular_map_ptr.reset(new EquirectangularMap(
      "resources/kloofendal_48d_partly_cloudy_puresky_4k.hdr", 2048));

  ReloadModel("resources/sponza/sponza.gltf", 40, 256);
  voxelization_visualization_ptr.reset(new vxgi::Visualization(width, height));

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(),
      [](uint32_t width, uint32_t height) {
        deferred_shading_render_quad_ptr->Resize(width, height);
        post_processes_ptr->Resize(width, height);
        voxelization_visualization_ptr->Resize(width, height);
      },
      width, height, window);

  ImGuiInit();
}

void RenderLoop() {
  glm::mat4 model_matrix = glm::scale(glm::mat4(1), glm::vec3(1));

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
        sprintf(buf, "VXGI Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    if (revoxelization) {
      // voxelization
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_BLEND);
      glViewport(0, 0, voxelization_ptr->voxel_resolution(),
                 voxelization_ptr->voxel_resolution());
      multi_draw_indirect->Draw(
          nullptr, nullptr, nullptr, false, voxelization_ptr.get(), false,
          false, {{model_ptr.get(), {{-1, 0, model_matrix, glm::vec4(0)}}}});
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);

      revoxelization = false;
    }

    // draw depth map first
    light_sources_ptr->DrawDepthForShadow(
        [&](int32_t directional_index, int32_t point_index) {
          multi_draw_indirect->DrawDepthForShadow(
              light_sources_ptr.get(), directional_index, point_index,
              {{model_ptr.get(), {{-1, 0, model_matrix, glm::vec4(0)}}}});
        });

    // light injection
    injection_ptr->Launch(voxelization_ptr->albedo(),
                          voxelization_ptr->normal(),
                          voxelization_ptr->metallic_and_roughness(),
                          camera_ptr.get(), light_sources_ptr.get());

    if (enable_voxelization_visualization) {
      voxelization_visualization_ptr->Draw(
          camera_ptr.get(), injection_ptr->light_injected(), 1,
          voxelization_ptr->world_size(), mipmap_level);
    } else {
      // deferred shading
      deferred_shading_render_quad_ptr->TwoPasses(
          camera_ptr.get(), light_sources_ptr.get(), enable_ssao,
          vxgi_config_ptr.get(),
          []() {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            equirectangular_map_ptr->skybox()->Draw(camera_ptr.get());
          },
          [&]() {
            glDisable(GL_CULL_FACE);
            multi_draw_indirect->Draw(
                camera_ptr.get(), nullptr, nullptr, true, nullptr,
                default_shading_choice, true,
                {{model_ptr.get(), {{-1, 0, model_matrix, glm::vec4(0)}}}});
          },
          post_processes_ptr->fbo());

      post_processes_ptr->Draw(nullptr);
    }

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