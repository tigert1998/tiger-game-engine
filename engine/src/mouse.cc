#include "mouse.h"

#include <GLFW/glfw3.h>

Mouse Mouse::shared = Mouse();

Mouse::Mouse() = default;

void Mouse::Trigger(int key, int action) {
  if (key < 0 || key >= Mouse::kTotal) key = Mouse::kTotal - 1;
  key_pressed_[key] = (action != GLFW_RELEASE);
  for (auto f : trigger_callbacks_) f(key, action);
}

void Mouse::Elapse(double delta_time) const {
  for (auto f : callbacks_) f(key_pressed_, delta_time, x_, y_);
}

void Mouse::Move(double x, double y) {
  x_ = x;
  y_ = y;
  for (auto f : callbacks_) f(key_pressed_, 0, x_, y_);
}

void Mouse::Register(
    std::function<void(MouseState, double, double, double)> yield) {
  callbacks_.push_back(yield);
}

void Mouse::Register(std::function<void(int, int)> callback) {
  trigger_callbacks_.push_back(callback);
}