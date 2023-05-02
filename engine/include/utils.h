#ifndef UTILS_H_
#define UTILS_H_

#include <assimp/matrix4x4.h>
#include <glog/logging.h>

#include <chrono>
#include <glm/glm.hpp>
#include <iomanip>
#include <ostream>
#include <string>

class Timer {
 private:
  std::string task_name_;
  std::chrono::high_resolution_clock::time_point start_;

 public:
  inline Timer(const std::string &task_name) : task_name_(task_name) {
    start_ = std::chrono::high_resolution_clock::now();
  }
  inline ~Timer() {
    auto now = std::chrono::high_resolution_clock::now();
    float ms =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_)
            .count() /
        1e3;
    LOG(INFO) << task_name_ << " consumes " << ms << "ms";
  }
};

template <typename T>
glm::mat4 Mat4FromAimatrix4x4(aiMatrix4x4t<T> matrix) {
  glm::mat4 res;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) res[j][i] = matrix[i][j];
  return res;
}

template <size_t W, size_t H, typename T, glm::qualifier Q>
std::ostream &operator<<(std::ostream &os, const glm::mat<W, H, T, Q> &mat) {
  for (int i = 0; i < H; i++) {
    for (int j = 0; j < W; j++) {
      os << std::setprecision(12) << mat[j][i] << ", ";
    }
    os << "\n";
  }
  return os;
}

template <size_t N, typename T, glm::qualifier Q>
std::ostream &operator<<(std::ostream &os, const glm::vec<N, T, Q> &vec) {
  os << "(";
  for (int i = 0; i < N; i++) {
    os << std::setprecision(12) << vec[i];
    if (i < N - 1) {
      os << ", ";
    }
  }
  os << ")";
  return os;
}

std::string BaseName(const std::string &path);

std::string ParentPath(const std::string &path);

std::string SnakeToPascal(const std::string &name);

void CheckOpenGLError();

#endif