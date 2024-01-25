// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glog/logging.h>
#include <imgui.h>

#include <glm/gtx/transform.hpp>
#include <iostream>
#include <memory>

#include "controller/sightseeing_controller.h"
#include "deferred_shading_render_quad.h"
#include "model.h"
#include "mouse.h"
#include "multi_draw_indirect.h"
#include "skybox.h"
#include "smaa.h"
#include "utils.h"

using namespace glm;
using namespace std;

class Controller : public SightseeingController {
 private:
  DeferredShadingRenderQuad *deferred_shading_render_quad_;
  SMAA *smaa_;

  void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
    SightseeingController::FramebufferSizeCallback(window, width, height);
    deferred_shading_render_quad_->Resize(width, height);
    smaa_->Resize(width, height);
  }

 public:
  Controller(Camera *camera,
             DeferredShadingRenderQuad *deferred_shading_render_quad,
             SMAA *smaa, uint32_t width, uint32_t height, GLFWwindow *window)
      : SightseeingController(camera, width, height, window),
        deferred_shading_render_quad_(deferred_shading_render_quad),
        smaa_(smaa) {
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow *window, int width, int height) {
          Controller *self = (Controller *)glfwGetWindowUserPointer(window);
          self->FramebufferSizeCallback(window, width, height);
        });
  }
};

std::unique_ptr<MultiDrawIndirect> multi_draw_indirect;
std::unique_ptr<DeferredShadingRenderQuad> deferred_shading_render_quad_ptr;
std::unique_ptr<SMAA> smaa_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<ShadowSources> shadow_sources_ptr;
std::unique_ptr<Skybox> skybox_ptr;
std::unique_ptr<Controller> controller_ptr;

double animation_time = 0;
int animation_id = 0;
int default_shading_choice = 0;
int enable_ssao = 0;
int enable_smaa = 0;

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
  // model
  char buf[1 << 10] = {0};
  static int prev_animation_id = animation_id;
  const char *choices[] = {"off", "on"};

  ImGui::Begin("Panel");
  ImGui::InputInt("animation id", &animation_id, 1, 1);
  ImGui::ListBox("default shading", &default_shading_choice, choices,
                 IM_ARRAYSIZE(choices));
  ImGui::ListBox("enable SSAO", &enable_ssao, choices, IM_ARRAYSIZE(choices));
  ImGui::ListBox("enable SMAA", &enable_smaa, choices, IM_ARRAYSIZE(choices));
  ImGui::End();

  // model
  if (prev_animation_id != animation_id) {
    if (0 <= animation_id && animation_id < model_ptr->NumAnimations()) {
      LOG(INFO) << "switching to animation #" << animation_id;
    } else {
      LOG(INFO) << "deactivate animation";
    }
    prev_animation_id = animation_id;
    animation_time = 0;
  }

  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow();
  shadow_sources_ptr->ImGuiWindow();
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Instancing Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  deferred_shading_render_quad_ptr.reset(
      new DeferredShadingRenderQuad(width, height));

  smaa_ptr.reset(new SMAA("./third_party/smaa", width, height));

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->Add(make_unique<Directional>(vec3(0, -1, 0.5), vec3(10)));
  light_sources_ptr->Add(make_unique<Ambient>(vec3(0.1)));

  multi_draw_indirect.reset(new MultiDrawIndirect());
  model_ptr.reset(new Model("resources/dragon/dragon.fbx",
                            multi_draw_indirect.get(), 9, true));
  multi_draw_indirect->PrepareForDraw();
  camera_ptr = make_unique<Camera>(
      vec3(7, 9, 0), static_cast<double>(width) / height,
      -glm::pi<double>() / 2, 0, glm::radians(60.f), 0.1, 500);
  camera_ptr->set_front(-camera_ptr->position());

  shadow_sources_ptr = make_unique<ShadowSources>(camera_ptr.get());

  skybox_ptr = make_unique<Skybox>("resources/skyboxes/cloud");

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), deferred_shading_render_quad_ptr.get(), smaa_ptr.get(),
      width, height, window);

  ImGuiInit();
}

std::vector<MultiDrawIndirect::RenderTargetParameter>
ConstructRenderTargetParameters() {
  MultiDrawIndirect::RenderTargetParameter param;
  param.model = model_ptr.get();
  for (int x = 0; x < 3; x++)
    for (int y = 0; y < 3; y++) {
      double item_animation_time = x * 3 + y + animation_time;
      double duration = model_ptr->AnimationDurationInSeconds(animation_id);
      if (duration > 0) {
        item_animation_time = item_animation_time -
                              floor(item_animation_time / duration) * duration;
      }
      glm::mat4 transform =
          glm::translate(glm::vec3((x - 1) * 5, (y - 1) * 5, 0)) *
          glm::scale(glm::vec3(0.05, 0.05, 0.05));
      param.items.push_back(
          {animation_id, item_animation_time, transform, glm::vec4(0)});
    }
  return {param};
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
        sprintf(buf, "Instancing Demo | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    auto render_target_params = ConstructRenderTargetParameters();

    // draw depth map first
    shadow_sources_ptr->DrawDepthForShadow([&](Shadow *shadow) {
      multi_draw_indirect->DrawDepthForShadow(shadow, render_target_params);
    });

    deferred_shading_render_quad_ptr->TwoPasses(
        camera_ptr.get(), light_sources_ptr.get(), shadow_sources_ptr.get(),
        enable_ssao,
        []() {
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          skybox_ptr->Draw(camera_ptr.get());
        },
        [&]() {
          multi_draw_indirect->Draw(camera_ptr.get(), nullptr, nullptr, nullptr,
                                    true, default_shading_choice, true,
                                    render_target_params);
        },
        enable_smaa ? smaa_ptr->fbo() : nullptr);

    if (enable_smaa) {
      smaa_ptr->Draw();
    }

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