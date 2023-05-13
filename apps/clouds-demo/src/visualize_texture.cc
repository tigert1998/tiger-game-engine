// clang-format off
#include <glad/glad.h>
// clang-format on

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glog/logging.h>
#include <imgui.h>

#include <glm/glm.hpp>
#include <iostream>
#include <memory>

#include "clouds/noise_texture_generator.h"
#include "shader.h"

struct TextureVisualizer {
 private:
  static const std::string kVsSource;
  static const std::string kGsSource;
  static const std::string kFsSource;

  uint32_t vao_;
  std::unique_ptr<Shader> shader_;

 public:
  TextureVisualizer() {
    shader_.reset(new Shader({
        {GL_VERTEX_SHADER, TextureVisualizer::kVsSource},
        {GL_GEOMETRY_SHADER, TextureVisualizer::kGsSource},
        {GL_FRAGMENT_SHADER, TextureVisualizer::kFsSource},
    }));
    glGenVertexArrays(1, &vao_);
  }

  void Draw(uint32_t target, uint32_t texture_id, float depth, uint32_t axis,
            uint32_t width, uint32_t height) {
    glBindVertexArray(vao_);
    shader_->Use();

    glActiveTexture(GL_TEXTURE0);
    if (target == GL_TEXTURE_2D) {
      glBindTexture(GL_TEXTURE_2D, texture_id);
      shader_->SetUniform<int32_t>("uSampler", 0);
      shader_->SetUniform<int32_t>("uIs3D", 0);
    } else if (target == GL_TEXTURE_3D) {
      glBindTexture(GL_TEXTURE_3D, texture_id);
      shader_->SetUniform<int32_t>("uSampler3D", 0);
      shader_->SetUniform<int32_t>("uIs3D", 1);
    }
    shader_->SetUniform<float>("uDepth", depth);
    shader_->SetUniform<uint32_t>("uAxis", axis);
    shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width, height));

    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glBindTexture(target, 0);
  }
};

const std::string TextureVisualizer::kVsSource = R"(
#version 410 core
void main() {}
)";

const std::string TextureVisualizer::kGsSource = R"(
#version 410 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

void main() {
    gl_Position = vec4(1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    EndPrimitive(); 
}
)";

const std::string TextureVisualizer::kFsSource = R"(
#version 410 core

uniform sampler2D uSampler;
uniform sampler3D uSampler3D;
uniform bool uIs3D;
uniform vec2 uScreenSize;
uniform uint uAxis;
uniform float uDepth;

out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / min(uScreenSize.x, uScreenSize.y);
    vec4 sampled;
    if (uIs3D) {
        sampled = texture(uSampler3D, vec3(uv, uDepth));
    } else {
        sampled = texture(uSampler, uv);
    }
    fragColor = vec4(vec3(clamp(sampled[uAxis], 0, 1)), 1); 
}
)";

GLFWwindow *window;

std::unique_ptr<TextureVisualizer> texture_visualizer;
std::unique_ptr<NoiseTextureGenerator> texture_generator;

uint32_t width = 1920, height = 1080;
float depth = 0;
int32_t axis = 0;
int32_t texture_item;

void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  ::width = width;
  ::height = height;
  glViewport(0, 0, width, height);
}

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
  ImGui::Begin("Panel");
  ImGui::DragFloat("depth", &depth, 0.01, 0, 1);
  ImGui::DragInt("axis", &axis, 0.05, 0, 3);
  static const char *texture_items[] = {"perlin_worley", "worley", "weather"};
  ImGui::ListBox("select texture", &texture_item, texture_items, 3);
  ImGui::End();
}

void Init(uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window =
      glfwCreateWindow(width, height, "Visualize Texture", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  ImGuiInit(width, height);

  texture_generator.reset(new NoiseTextureGenerator(0.92));
  texture_visualizer.reset(new TextureVisualizer());
}

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

  Init(width, height);

  while (!glfwWindowShouldClose(window)) {
    static uint32_t fps = 0;
    static double last_time_for_fps = glfwGetTime();
    static double last_time = glfwGetTime();
    double current_time = glfwGetTime();
    double delta_time = current_time - last_time;
    last_time = current_time;

    {
      fps += 1;
      if (current_time - last_time_for_fps >= 1.0) {
        char buf[1 << 10];
        sprintf(buf, "Visualize Texture | FPS: %d\n", fps);
        glfwSetWindowTitle(window, buf);
        fps = 0;
        last_time_for_fps = current_time;
      }
    }

    glfwPollEvents();

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (texture_item == 0) {
      texture_visualizer->Draw(GL_TEXTURE_3D,
                               texture_generator->perlin_worley_texture_id(),
                               depth, axis, width, height);
    } else if (texture_item == 1) {
      texture_visualizer->Draw(GL_TEXTURE_3D,
                               texture_generator->worley_texture_id(), depth,
                               axis, width, height);
    } else if (texture_item == 2) {
      texture_visualizer->Draw(GL_TEXTURE_2D,
                               texture_generator->weather_texture_id(), depth,
                               axis, width, height);
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