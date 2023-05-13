#ifndef CONTROLLER_SIGHTSEEING_CONTROLLER_H_
#define CONTROLLER_SIGHTSEEING_CONTROLLER_H_

#include <GLFW/glfw3.h>
#include <stdint.h>

#include "camera.h"
#include "keyboard.h"
#include "mouse.h"

class SightseeingController {
 private:
  Camera *camera_;
  uint32_t width_, height_;
  GLFWwindow *window_;
  bool rotating_camera_mode_;

 protected:
  void MouseButtonCallback(GLFWwindow *window, int button, int action, int);
  void CursorPosCallback(GLFWwindow *window, double x, double y);
  void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);
  void KeyCallback(GLFWwindow *window, int key, int, int action, int);
  void FramebufferSizeCallback(GLFWwindow *window, int width, int height);

 public:
  SightseeingController(Camera *camera, uint32_t width, uint32_t height,
                        GLFWwindow *window);
};

#endif