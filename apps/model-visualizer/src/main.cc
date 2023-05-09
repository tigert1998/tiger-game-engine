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

#include "model.h"
#include "mouse.h"
#include "skybox.h"

uint32_t width = 1920, height = 1080;

using namespace glm;
using namespace std;

std::unique_ptr<OITRenderQuad> oit_render_quad_ptr;
std::unique_ptr<Model> model_ptr;
std::unique_ptr<Camera> camera_ptr;
std::unique_ptr<LightSources> light_sources_ptr;
std::unique_ptr<Skybox> skybox_ptr;

double animation_time = 0;
int animation_id = -1;
float scroll_ratio = 10;

GLFWwindow *window;

void MouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  Mouse::shared.Trigger(button, action);
}

void CursorPosCallback(GLFWwindow *window, double x, double y) {
  Mouse::shared.Move(x, y);
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  auto position = camera_ptr->position();
  double distance =
      glm::distance(position, glm::vec3(0)) + yoffset * scroll_ratio;
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
  oit_render_quad_ptr->Resize(width, height);
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
  char buf[1 << 10] = {0};
  static int prev_animation_id = animation_id;
  const char *default_shading_choices[] = {"off", "on"};
  int default_shading_choice = model_ptr->default_shading() ? 1 : 0;

  if (prev_animation_id != animation_id) {
    if (0 <= animation_id && animation_id < model_ptr->NumAnimations()) {
      LOG(INFO) << "switching to animation #" << animation_id;
    } else {
      LOG(INFO) << "deactivate animation";
    }
    prev_animation_id = animation_id;
    animation_time = 0;
  }

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
  ImGui::InputFloat("scroll ratio", &scroll_ratio);
  ImGui::End();

  camera_ptr->set_position(vec3(p_arr[0], p_arr[1], p_arr[2]));
  camera_ptr->set_front(vec3(f_arr[0], f_arr[1], f_arr[2]));
  camera_ptr->set_alpha(alpha);
  camera_ptr->set_beta(beta);
  model_ptr->set_default_shading(default_shading_choice == 1);
}

void Init() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window =
      glfwCreateWindow(width, height, "Model Visualizer", nullptr, nullptr);
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

  oit_render_quad_ptr = make_unique<OITRenderQuad>(width, height);

  light_sources_ptr = make_unique<LightSources>();
  light_sources_ptr->Add(
      make_unique<Directional>(vec3(0, -1, -1), vec3(1, 1, 1)));
  light_sources_ptr->Add(make_unique<Ambient>(vec3(0.4)));

  model_ptr = make_unique<Model>("resources/sponza/sponza.obj",
                                 oit_render_quad_ptr.get());
  camera_ptr = make_unique<Camera>(vec3(850, 300, 0),
                                   static_cast<double>(width) / height);
  camera_ptr->set_front(-camera_ptr->position());
  skybox_ptr = make_unique<Skybox>("resources/skyboxes/cloud", "png");

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
    animation_time += delta_time;

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

    oit_render_quad_ptr->BindFrameBuffer();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    skybox_ptr->Draw(camera_ptr.get());
    oit_render_quad_ptr->UnBindFrameBuffer();

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    oit_render_quad_ptr->CopyDepthToDefaultFrameBuffer();
    glDepthMask(GL_FALSE);
    oit_render_quad_ptr->ResetBeforeRender();
    if (animation_id < 0 || animation_id >= model_ptr->NumAnimations()) {
      model_ptr->Draw(camera_ptr.get(), light_sources_ptr.get(), mat4(1));
    } else {
      model_ptr->Draw(0, animation_time, camera_ptr.get(),
                      light_sources_ptr.get(), mat4(1), vec4(0));
    }
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
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