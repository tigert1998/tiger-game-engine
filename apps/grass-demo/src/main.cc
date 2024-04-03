// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fmt/core.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "controller/sightseeing_controller.h"
#include "grass/grassland.h"
#include "skybox.h"
#include "tone_mapping/aces.h"

std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Grassland> grassland_ptr;
std::unique_ptr<PostProcesses> post_processes_ptr;
std::unique_ptr<Skybox> skybox_ptr;
std::unique_ptr<SightseeingController> controller;

GLFWwindow *window;

void ImGuiInit(uint32_t width, uint32_t height) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = width;
  io.DisplaySize.y = height;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsClassic();
  auto font = io.Fonts->AddFontDefault();
  io.Fonts->Build();
}

void ImGuiWindow() {
  camera_ptr->ImGuiWindow();
  post_processes_ptr->ImGuiWindow();
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Grass Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  post_processes_ptr.reset(new PostProcesses());
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::ACES>(
      new tone_mapping::ACES(width, height)));

  camera_ptr = std::make_unique<Camera>(
      glm::vec3(0, 10, 0), static_cast<double>(width) / height,
      -0.5 * glm::pi<float>(), 0, glm::radians(60.0f), 0.1, 5000);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->AddDirectional(std::make_unique<DirectionalLight>(
      glm::vec3(0, -1, -1), glm::vec3(1, 1, 1), camera_ptr.get()));
  light_sources_ptr->AddAmbient(std::make_unique<AmbientLight>(glm::vec3(0.4)));

  skybox_ptr = std::make_unique<Skybox>("resources/skyboxes/cloud");
  grassland_ptr = std::make_unique<Grassland>("resources/terrain/sample.obj",
                                              "resources/distortion.png");

  controller.reset(
      new SightseeingController(camera_ptr.get(), width, height, window));

  ImGuiInit(width, height);
}

int main(int argc, char *argv[]) {
  try {
    Init(1920, 1080);
  } catch (const std::exception &e) {
    fmt::print(stderr, "[error] {}\n", e.what());
    exit(1);
  }

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
        sprintf(buf, "Grass Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    post_processes_ptr->fbo()->Bind();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    skybox_ptr->Draw(camera_ptr.get());
    grassland_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(),
                        current_time);
    post_processes_ptr->fbo()->Unbind();
    post_processes_ptr->Draw(nullptr);

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuiWindow();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  return 0;
}