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

#include "clouds/clouds.h"
#include "controller/sightseeing_controller.h"
#include "light_sources.h"
#include "model.h"
#include "oit_render_quad.h"
#include "shadow_sources.h"

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
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Clouds> clouds_ptr;
std::unique_ptr<ShadowSources> shadow_sources_ptr;

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

  window = glfwCreateWindow(width, height, "Clouds Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  light_sources_ptr = std::make_unique<LightSources>();
  light_sources_ptr->Add(
      std::make_unique<Directional>(glm::vec3(0, -1, -1), glm::vec3(1, 1, 1)));
  light_sources_ptr->Add(std::make_unique<Ambient>(glm::vec3(0.04)));

  shadow_sources_ptr = std::make_unique<ShadowSources>();

  camera_ptr = std::make_unique<Camera>(
      glm::vec3(0, 10, 0), static_cast<double>(width) / height,
      -0.5 * glm::pi<float>(), 0, glm::radians(60.f), 0.1, 5000);

  oit_render_quad_ptr.reset(new OITRenderQuad(width, height));
  model_ptr.reset(
      new Model("resources/sphere/sphere.obj", oit_render_quad_ptr.get()));
  clouds_ptr.reset(new Clouds(width, height));

  controller.reset(new Controller(camera_ptr.get(), oit_render_quad_ptr.get(),
                                  clouds_ptr.get(), width, height, window));

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
        sprintf(buf, "Clouds Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    oit_render_quad_ptr->TwoPasses(
        controller->width(), controller->height(),
        []() {
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        },
        []() {
          model_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(),
                          shadow_sources_ptr.get(), glm::mat4(1));
        },
        clouds_ptr->fbo());

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