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

#include "clouds/clouds.h"
#include "controller/sightseeing_controller.h"
#include "light_sources.h"
#include "model.h"
#include "oit_render_quad.h"
#include "shadows/shadow.h"

class Controller : public SightseeingController {
 private:
  OITRenderQuad *oit_render_quad_;
  Clouds *clouds_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    oit_render_quad_->Resize(width, height);
    clouds_->Resize(width, height);
  }

 public:
  Controller(Camera *camera, OITRenderQuad *oit_render_quad, Clouds *clouds,
             uint32_t width, uint32_t height, GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        oit_render_quad_(oit_render_quad),
        clouds_(clouds) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<OITRenderQuad> oit_render_quad_ptr;
std::unique_ptr<Controller> controller;
std::unique_ptr<Clouds> clouds_ptr;

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
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Clouds Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  camera_ptr = std::make_unique<Camera>(
      glm::vec3(0, 10, 0), static_cast<double>(width) / height,
      -0.5 * glm::pi<float>(), 0, glm::radians(60.f), 0.1, 5000);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->AddDirectional(std::make_unique<DirectionalLight>(
      glm::vec3(0, -1, -1), glm::vec3(1), camera_ptr.get()));
  light_sources_ptr->AddAmbient(
      std::make_unique<AmbientLight>(glm::vec3(0.04)));

  oit_render_quad_ptr.reset(new OITRenderQuad(width, height));
  clouds_ptr.reset(new Clouds(width, height));

  controller.reset(new Controller(camera_ptr.get(), oit_render_quad_ptr.get(),
                                  clouds_ptr.get(), width, height, window));

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
        sprintf(buf, "Clouds Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    oit_render_quad_ptr->TwoPasses(
        []() {
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        },
        []() {}, clouds_ptr->fbo());

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    clouds_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(), current_time);

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