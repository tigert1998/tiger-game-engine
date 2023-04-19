#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include <bitset>
#include <functional>
#include <vector>

class Keyboard {
 private:
  static constexpr int kTotal = 1024;
  std::bitset<kTotal> key_pressed_;
  std::vector<std::function<void(std::bitset<kTotal>, double)>> callbacks_;

 public:
  using KeyboardState = std::bitset<kTotal>;
  static Keyboard shared;
  Keyboard();
  void Trigger(int key, int action);
  void Elapse(double time) const;
  void Register(std::function<void(KeyboardState, double)> callback);
};

#endif