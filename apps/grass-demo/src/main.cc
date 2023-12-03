// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glog/logging.h>
#include <imgui.h>

#include <iostream>
#include <memory>

#include "controller/sightseeing_controller.h"
#include "grass/grassland.h"
#include "skybox.h"

std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Grassland> grassland_ptr;
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
  auto p = camera_ptr->position();
  auto f = camera_ptr->front();
  float p_arr[3] = {p.x, p.y, p.z};
  float f_arr[3] = {f.x, f.y, f.z};
  float alpha = camera_ptr->alpha();
  float beta = camera_ptr->beta();

  ImGui::Begin("Panel");
  ImGui::InputFloat3("camera.position", p_arr);
  ImGui::InputFloat3("camera.front", f_arr);
  ImGui::InputFloat("camera.alpha", &alpha);
  ImGui::InputFloat("camera.beta", &beta);
  ImGui::End();

  camera_ptr->set_position(glm::vec3(p_arr[0], p_arr[1], p_arr[2]));
  camera_ptr->set_front(glm::vec3(f_arr[0], f_arr[1], f_arr[2]));
  camera_ptr->set_alpha(alpha);
  camera_ptr->set_beta(beta);
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Grass Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->Add(
      std::make_unique<Directional>(glm::vec3(0, -1, -1), glm::vec3(1, 1, 1)));
  light_sources_ptr->Add(std::make_unique<Ambient>(glm::vec3(0.4)));

  camera_ptr = std::make_unique<Camera>(
      glm::vec3(0, 10, 0), static_cast<double>(width) / height,
      -0.5 * glm::pi<float>(), 0, glm::radians(60.0f), 0.1, 5000);
  skybox_ptr = std::make_unique<Skybox>("resources/skyboxes/cloud", "png");
  grassland_ptr = std::make_unique<Grassland>("resources/terrain/sample.obj",
                                              "resources/distortion.png");

  controller.reset(
      new SightseeingController(camera_ptr.get(), width, height, window));

  ImGuiInit(width, height);
}

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  Init(1920, 1080);

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

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    skybox_ptr->Draw(camera_ptr.get());

    grassland_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(),
                        current_time);

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