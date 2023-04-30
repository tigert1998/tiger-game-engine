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

#include "grass/grassland.h"
#include "mouse.h"
#include "skybox.h"

std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Grassland> grassland_ptr;
std::unique_ptr<Skybox> skybox_ptr;

GLFWwindow *window;

uint32_t width = 1920, height = 1080;

void MouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  Mouse::shared.Trigger(button, action);
}

void CursorPosCallback(GLFWwindow *window, double x, double y) {
  Mouse::shared.Move(x, y);
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  auto position = camera_ptr->position();
  double distance = glm::distance(position, glm::vec3(0)) + yoffset * 0.2;
  distance = std::max(distance, 0.1);
  position = glm::normalize(position) * (float)distance;
  camera_ptr->set_position(position);
}

void KeyCallback(GLFWwindow *window, int key, int, int action, int) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  ::width = width;
  ::height = height;
  camera_ptr->set_width_height_ratio(static_cast<double>(width) / height);
  glViewport(0, 0, width, height);
}

void ImGuiInit() {
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

void Init() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Grass Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glfwSetScrollCallback(window, ScrollCallback);

  Mouse::shared.Register(
      [](Mouse::MouseState state, double delta, double x, double y) {
        static double lastx = 0, lasty = 0;
        if (state[GLFW_MOUSE_BUTTON_LEFT]) {
          double normalized_x = (x - lastx) / width;
          double normalized_y = (y - lasty) / height;

          auto position = camera_ptr->position();
          double distance = glm::distance(position, glm::vec3(0));
          double theta = atan2(-position.z, position.x) - normalized_x * 2;
          double gamma = -camera_ptr->beta() + normalized_y;

          position.x = distance * cos(theta) * cos(gamma);
          position.z = -distance * sin(theta) * cos(gamma);
          position.y = distance * sin(gamma);

          camera_ptr->set_position(position);
          camera_ptr->set_front(-camera_ptr->position());
        }
        lastx = x;
        lasty = y;
      });

  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->Add(
      std::make_unique<Directional>(glm::vec3(0, -1, -1), glm::vec3(1, 1, 1)));
  light_sources_ptr->Add(std::make_unique<Ambient>(glm::vec3(0.4)));

  camera_ptr = std::make_unique<Camera>(glm::vec3(0.5, 0.25, 1),
                                        static_cast<double>(width) / height);
  camera_ptr->set_front(-camera_ptr->position());
  skybox_ptr = std::make_unique<Skybox>("resources/skyboxes/cloud", "png");
  grassland_ptr = std::make_unique<Grassland>("resources/terrain/sample.obj");

  ImGuiInit();
}

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  Init();

  while (!glfwWindowShouldClose(window)) {
    static double last_time = glfwGetTime();
    double current_time = glfwGetTime();
    double delta_time = current_time - last_time;
    last_time = current_time;

    glfwPollEvents();

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    skybox_ptr->Draw(camera_ptr.get());

    grassland_ptr->Draw(camera_ptr.get(), light_sources_ptr.get());

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