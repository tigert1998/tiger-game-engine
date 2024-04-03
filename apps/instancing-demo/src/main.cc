// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fmt/core.h>
#include <imgui.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <memory>
#include <string>

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
std::unique_ptr<PostProcesses> post_processes_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Skybox> skybox_ptr;
std::unique_ptr<Controller> controller_ptr;

const int kNumModelItems = 12;

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
      fmt::print(stderr, "[info] switching to animation #{}\n", animation_id);
    } else {
      fmt::print(stderr, "[info] deactivate animation\n");
    }
    prev_animation_id = animation_id;
    animation_time = 0;
  }

  camera_ptr->ImGuiWindow();
  light_sources_ptr->ImGuiWindow(camera_ptr.get());
  post_processes_ptr->ImGuiWindow();

  post_processes_ptr->Enable(1, enable_smaa);
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window = glfwCreateWindow(width, height, "Instancing Demo", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Shader::include_directories = {"./shaders"};

  deferred_shading_render_quad_ptr.reset(
      new DeferredShadingRenderQuad(width, height));

  post_processes_ptr.reset(new PostProcesses());
  post_processes_ptr->Add(std::unique_ptr<tone_mapping::ACES>(
      new tone_mapping::ACES(width, height)));
  post_processes_ptr->Add(
      std::unique_ptr<SMAA>(new SMAA("./third_party/smaa", width, height)));

  camera_ptr = make_unique<Camera>(vec3(0.087, 8.209, 31.708),
                                   static_cast<double>(width) / height, -1.687,
                                   -0.281, glm::radians(60.f), 0.1, 500);
  camera_ptr->set_front(-camera_ptr->position());

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->AddDirectional(
      make_unique<DirectionalLight>(vec3(0, 0, -1), vec3(2), camera_ptr.get()));
  light_sources_ptr->AddAmbient(make_unique<AmbientLight>(vec3(0.1)));

  model_ptr.reset(new Model(
      "resources/Tarisland - Dragon/source/M_B_44_Qishilong_skin_Skeleton.FBX",
      true, false));
  multi_draw_indirect.reset(new MultiDrawIndirect());
  model_ptr->SubmitToMultiDrawIndirect(multi_draw_indirect.get(),
                                       kNumModelItems);
  multi_draw_indirect->PrepareForDraw();

  skybox_ptr = make_unique<Skybox>("resources/skyboxes/learnopengl");

  controller_ptr = make_unique<Controller>(
      camera_ptr.get(), deferred_shading_render_quad_ptr.get(),
      post_processes_ptr.get(), width, height, window);

  ImGuiInit();
}

std::vector<MultiDrawIndirect::RenderTargetParameter>
ConstructRenderTargetParameters() {
  MultiDrawIndirect::RenderTargetParameter param;
  param.model = model_ptr.get();
  auto get_xy = [](int i, int num_samples) -> glm::vec2 {
    float pi = glm::pi<float>();
    double t = 2 * pi / num_samples * i;
    float x = 16 * pow(sin(t), 3);
    float y = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t);
    return {x, y};
  };

  static const std::vector<glm::vec2> xys = [&](int num_models) {
    int num_samples = 4096;
    std::vector<glm::vec2> xys;
    for (int i = 0; i < num_samples; i++) xys.push_back(get_xy(i, num_samples));
    float distance = 0;
    for (int i = 0; i < num_samples; i++)
      distance += glm::distance(xys[i], xys[(i + 1) % num_samples]);
    float distance_between_models = distance / num_models;
    std::vector<glm::vec2> ret;
    ret.push_back(xys[0]);
    float tmp = 0;
    for (int i = 0, j = 0; i < num_samples; i++) {
      tmp += glm::distance(xys[i], xys[(i + 1) % num_samples]);
      if (tmp >= distance_between_models) {
        tmp = 0;
        j = (i + 1) % num_samples;
        if (ret.size() < num_models) ret.push_back(xys[j]);
      }
    }
    return ret;
  }(kNumModelItems);

  for (int i = 0; i < xys.size(); i++) {
    auto xy = xys[i];
    double item_animation_time = i + animation_time;
    double duration = model_ptr->AnimationDurationInSeconds(animation_id);
    if (duration > 0) {
      item_animation_time = item_animation_time -
                            floor(item_animation_time / duration) * duration;
    }
    glm::mat4 transform = glm::translate(glm::vec3(xy[0], xy[1], 0)) *
                          glm::scale(glm::vec3(0.1f)) *
                          glm::rotate(glm::radians(90.f), glm::vec3(1, 0, 0));
    param.items.push_back(
        {animation_id, item_animation_time, transform, glm::vec4(0)});
  }
  return {param};
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
    light_sources_ptr->DrawDepthForShadow([&](int32_t directional_index,
                                              int32_t point_index) {
      multi_draw_indirect->DrawDepthForShadow(light_sources_ptr.get(),
                                              directional_index, point_index,
                                              render_target_params);
    });

    deferred_shading_render_quad_ptr->TwoPasses(
        camera_ptr.get(), light_sources_ptr.get(), enable_ssao, nullptr,
        []() {
          glEnable(GL_CULL_FACE);
          glCullFace(GL_BACK);
          glClearColor(0, 0, 0, 1);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          skybox_ptr->Draw(camera_ptr.get());
        },
        [&]() {
          glDisable(GL_CULL_FACE);
          multi_draw_indirect->Draw(camera_ptr.get(), nullptr, nullptr, true,
                                    nullptr, default_shading_choice, true,
                                    render_target_params);
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
  return 0;
}