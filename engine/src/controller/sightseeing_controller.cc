// clang-format off
#include <glad/glad.h>
// clang-format on

#include "controller/sightseeing_controller.h"

SightseeingController::SightseeingController(Camera *camera, uint32_t width,
                                             uint32_t height,
                                             GLFWwindow *window)
    : camera_(camera),
      width_(width),
      height_(height),
      window_(window),
      rotating_camera_mode_(false) {
  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, [](GLFWwindow *window, int key, int scancode,
                                 int action, int mods) {
    SightseeingController *self =
        (SightseeingController *)glfwGetWindowUserPointer(window);
    self->KeyCallback(window, key, scancode, action, mods);
  });
  glfwSetMouseButtonCallback(
      window_, [](GLFWwindow *window, int button, int action, int mods) {
        SightseeingController *self =
            (SightseeingController *)glfwGetWindowUserPointer(window);
        self->MouseButtonCallback(window, button, action, mods);
      });
  glfwSetCursorPosCallback(window_, [](GLFWwindow *window, double x, double y) {
    SightseeingController *self =
        (SightseeingController *)glfwGetWindowUserPointer(window);
    self->CursorPosCallback(window, x, y);
  });
  glfwSetScrollCallback(
      window_, [](GLFWwindow *window, double xoffset, double yoffset) {
        SightseeingController *self =
            (SightseeingController *)glfwGetWindowUserPointer(window);
        self->ScrollCallback(window, xoffset, yoffset);
      });
  glfwSetFramebufferSizeCallback(
      window_, [](GLFWwindow *window, int width, int height) {
        SightseeingController *self =
            (SightseeingController *)glfwGetWindowUserPointer(window);
        self->FramebufferSizeCallback(window, width, height);
      });

  Mouse::shared.Register([this](int key, int action) {
    if (key == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
      this->rotating_camera_mode_ = !this->rotating_camera_mode_;
      glfwSetInputMode(this->window_, GLFW_CURSOR,
                       this->rotating_camera_mode_ ? GLFW_CURSOR_DISABLED
                                                   : GLFW_CURSOR_NORMAL);
    }
  });

  Mouse::shared.Register(
      [this](Mouse::MouseState state, double delta, double x, double y) {
        static double lastx = 0, lasty = 0;
        if (this->rotating_camera_mode_) {
          double normalized_x = (x - lastx) / this->width_;
          double normalized_y = (y - lasty) / this->height_;

          this->camera_->set_alpha(this->camera_->alpha() + normalized_x * 2);
          this->camera_->set_beta(this->camera_->beta() - normalized_y);
        }
        lastx = x;
        lasty = y;
      });

  Keyboard::shared.Register([this](Keyboard::KeyboardState state,
                                   double delta) {
    if (state[GLFW_KEY_ESCAPE]) {
      glfwSetWindowShouldClose(this->window_, GL_TRUE);
    } else if (this->rotating_camera_mode_) {
      float distance = delta * 10;
      if (state[GLFW_KEY_W])
        this->camera_->Move(Camera::MoveDirectionType::kForward, distance);
      if (state[GLFW_KEY_S])
        this->camera_->Move(Camera::MoveDirectionType::kBackward, distance);
      if (state[GLFW_KEY_A])
        this->camera_->Move(Camera::MoveDirectionType::kLeftward, distance);
      if (state[GLFW_KEY_D])
        this->camera_->Move(Camera::MoveDirectionType::kRightward, distance);
    }
  });
}

void SightseeingController::MouseButtonCallback(GLFWwindow *window, int button,
                                                int action, int) {
  Mouse::shared.Trigger(button, action);
}

void SightseeingController::CursorPosCallback(GLFWwindow *window, double x,
                                              double y) {
  Mouse::shared.Move(x, y);
}

void SightseeingController::ScrollCallback(GLFWwindow *window, double xoffset,
                                           double yoffset) {}

void SightseeingController::KeyCallback(GLFWwindow *window, int key, int,
                                        int action, int) {
  Keyboard::shared.Trigger(key, action);
}

void SightseeingController::FramebufferSizeCallback(GLFWwindow *window,
                                                    int width, int height) {
  width_ = width;
  height_ = height;
  camera_->set_width_height_ratio(static_cast<double>(width) / height);
  glViewport(0, 0, width, height);
}