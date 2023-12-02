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
#include "model.h"
#include "mouse.h"
#include "skybox.h"

using namespace glm;
using namespace std;

class Controller : public SightseeingController {
 private:
  OITRenderQuad *oit_render_quad_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    oit_render_quad_->Resize(width, height);
  }

 public:
  Controller(Camera *camera, OITRenderQuad *oit_render_quad, uint32_t width,
             uint32_t height, GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        oit_render_quad_(oit_render_quad) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<OITRenderQuad> oit_render_quad_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<ShadowSources> shadow_sources_ptr;
std::unique_ptr<Skybox> skybox_ptr;
std::unique_ptr<Controller> controller_ptr;

double animation_time = 0;
int animation_id = -1;

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

void ImGuiWindow() {
  // camera
  auto p = camera_ptr->position();
  auto f = camera_ptr->front();
  float p_arr[3] = {p.x, p.y, p.z};
  float f_arr[3] = {f.x, f.y, f.z};
  float alpha = camera_ptr->alpha();
  float beta = camera_ptr->beta();

  // model
  char buf[1 << 10] = {0};
  static int prev_animation_id = animation_id;
  const char *default_shading_choices[] = {"off", "on"};
  int default_shading_choice = model_ptr->default_shading() ? 1 : 0;

  // shadow
  DirectionalShadow *shadow =
      dynamic_cast<DirectionalShadow *>(shadow_sources_ptr->Get(0));
  auto shadow_d = shadow->direction();
  float shadow_d_arr[3] = {shadow_d.x, shadow_d.y, shadow_d.z};

  ImGui::Begin("Panel");
  ImGui::InputFloat3("camera.position", p_arr);
  ImGui::InputFloat3("camera.front", f_arr);
  ImGui::InputFloat("camera.alpha", &alpha);
  ImGui::InputFloat("camera.beta", &beta);
  if (ImGui::InputText("model path", buf, sizeof(buf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    LOG(INFO) << "loading model: " << buf;
    model_ptr.reset(new Model(buf, oit_render_quad_ptr.get()));
    animation_time = 0;
  }
  ImGui::InputInt("animation id", &animation_id, 1, 1);
  ImGui::ListBox("default shading", &default_shading_choice,
                 default_shading_choices,
                 IM_ARRAYSIZE(default_shading_choices));
  ImGui::InputFloat3("shadow.direction", shadow_d_arr);
  ImGui::End();

  // camera
  camera_ptr->set_position(vec3(p_arr[0], p_arr[1], p_arr[2]));
  camera_ptr->set_front(vec3(f_arr[0], f_arr[1], f_arr[2]));
  camera_ptr->set_alpha(alpha);
  camera_ptr->set_beta(beta);
  // model
  model_ptr->set_default_shading(default_shading_choice == 1);
  if (prev_animation_id != animation_id) {
    if (0 <= animation_id && animation_id < model_ptr->NumAnimations()) {
      LOG(INFO) << "switching to animation #" << animation_id;
    } else {
      LOG(INFO) << "deactivate animation";
    }
    prev_animation_id = animation_id;
    animation_time = 0;
  }
  // shadow
  shadow->set_direction(
      vec3(shadow_d_arr[0], shadow_d_arr[1], shadow_d_arr[2]));

  light_sources_ptr->ImGuiWindow();
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window =
      glfwCreateWindow(width, height, "Model Visualizer", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  oit_render_quad_ptr = make_unique<OITRenderQuad>(width, height);

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->Add(make_unique<Directional>(vec3(0, -1, 0.1), vec3(10)));
  light_sources_ptr->Add(make_unique<Ambient>(vec3(0.1)));

  model_ptr = make_unique<Model>("resources/sponza/Sponza.gltf",
                                 oit_render_quad_ptr.get());
  camera_ptr =
      make_unique<Camera>(vec3(7, 9, 0), static_cast<double>(width) / height);
  camera_ptr->set_front(-camera_ptr->position());

  shadow_sources_ptr = make_unique<ShadowSources>();
  shadow_sources_ptr->Add(make_unique<DirectionalShadow>(
      vec3(0, -1, 0.1), 1024, 1024, camera_ptr.get()));

  skybox_ptr = make_unique<Skybox>("resources/skyboxes/cloud", "png");

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), oit_render_quad_ptr.get(), width, height, window);

  ImGuiInit();
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
    animation_time += delta_time;

    Keyboard::shared.Elapse(delta_time);

    {
      fps += 1;
      if (current_time - last_time_for_fps >= 1.0) {
        char buf[1 << 10];
        sprintf(buf, "Model Visualizer | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    // draw depth map first
    shadow_sources_ptr->DrawDepthForShadow([](Shadow *shadow) {
      if (animation_id < 0 || animation_id >= model_ptr->NumAnimations()) {
        model_ptr->DrawDepthForShadow(shadow, mat4(1));
      } else {
        model_ptr->DrawDepthForShadow(animation_id, animation_time, shadow,
                                      mat4(1));
      }
    });

    oit_render_quad_ptr->Bind();
    // draw background for OIT render quad
    // all opaque objects must be draw here
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    skybox_ptr->Draw(camera_ptr.get());
    oit_render_quad_ptr->Unbind();

    // append transparent objects to the linked list
    glViewport(0, 0, controller_ptr->width(), controller_ptr->height());
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDepthMask(GL_FALSE);
    oit_render_quad_ptr->ResetBeforeRender();
    if (animation_id < 0 || animation_id >= model_ptr->NumAnimations()) {
      model_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(),
                      shadow_sources_ptr.get(), mat4(1));
    } else {
      model_ptr->Draw(animation_id, animation_time, camera_ptr.get(),
                      light_sources_ptr.get(), shadow_sources_ptr.get(),
                      mat4(1), vec4(0));
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    // draw OIT render quad
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    oit_render_quad_ptr->Draw();

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