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
#include "keyboard.h"
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
  camera_ptr->Move(Camera::MoveDirectionType::kUpward, yoffset * 0.2);
}

void KeyCallback(GLFWwindow *window, int key, int, int action, int) {
  Keyboard::shared.Trigger(key, action);
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

          camera_ptr->set_alpha(camera_ptr->alpha() - normalized_x * 2);
          camera_ptr->set_beta(camera_ptr->beta() + normalized_y);
        }
        lastx = x;
        lasty = y;
      });

  Keyboard::shared.Register([](Keyboard::KeyboardState state, double delta) {
    if (state[GLFW_KEY_ESCAPE]) {
      glfwSetWindowShouldClose(window, GL_TRUE);
    } else {
      float distance = delta * 10;
      if (state[GLFW_KEY_W])
        camera_ptr->Move(Camera::MoveDirectionType::kForward, distance);
      if (state[GLFW_KEY_S])
        camera_ptr->Move(Camera::MoveDirectionType::kBackward, distance);
      if (state[GLFW_KEY_A])
        camera_ptr->Move(Camera::MoveDirectionType::kLeftward, distance);
      if (state[GLFW_KEY_D])
        camera_ptr->Move(Camera::MoveDirectionType::kRightward, distance);
    }
  });

  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->Add(
      std::make_unique<Directional>(glm::vec3(0, -1, -1), glm::vec3(1, 1, 1)));
  light_sources_ptr->Add(std::make_unique<Ambient>(glm::vec3(0.4)));

  camera_ptr = std::make_unique<Camera>(glm::vec3(0, 10, 0),
                                        static_cast<double>(width) / height);
  camera_ptr->set_alpha(-0.5 * glm::pi<float>());
  camera_ptr->set_beta(0);
  skybox_ptr = std::make_unique<Skybox>("resources/skyboxes/cloud", "png");
  grassland_ptr = std::make_unique<Grassland>("resources/terrain/sample.obj",
                                              "resources/distortion.png");

  ImGuiInit();
}

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  Init();

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