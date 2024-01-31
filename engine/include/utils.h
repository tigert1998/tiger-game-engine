#ifndef UTILS_H_
#define UTILS_H_

#include <assimp/matrix4x4.h>
#include <assimp/types.h>
#include <fmt/core.h>

#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <iomanip>
#include <ostream>
#include <source_location>
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
    fmt::print(stderr, "[info] {} consumes {} ms\n", task_name_, ms);
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

std::string SnakeToPascal(const std::string &name);

#define CHECK_OPENGL_ERROR()                                           \
  do {                                                                 \
    auto err = glGetError();                                           \
    if (err != GL_NO_ERROR) {                                          \
      std::string err_str;                                             \
      switch (err) {                                                   \
        case GL_INVALID_ENUM:                                          \
          err_str = "GL_INVALID_ENUM";                                 \
          break;                                                       \
        case GL_INVALID_VALUE:                                         \
          err_str = "GL_INVALID_VALUE";                                \
          break;                                                       \
        case GL_INVALID_OPERATION:                                     \
          err_str = "GL_INVALID_OPERATION";                            \
          break;                                                       \
        case GL_OUT_OF_MEMORY:                                         \
          err_str = "GL_OUT_OF_MEMORY";                                \
          break;                                                       \
        case GL_INVALID_FRAMEBUFFER_OPERATION:                         \
          err_str = "GL_INVALID_FRAMEBUFFER_OPERATION";                \
          break;                                                       \
        default:                                                       \
          err_str = std::to_string(err);                               \
      }                                                                \
      auto location = std::source_location::current();                 \
      fmt::print(stderr, "[{}({}), error] {}\n", location.file_name(), \
                 location.line(), err_str);                            \
      exit(1);                                                         \
    }                                                                  \
  } while (0)

std::string ReadFile(const std::filesystem::path &file_path, bool binary);

std::string ToLower(const std::string &str);

std::u8string ToU8string(const aiString &str);

std::string StringToHex(const std::string &input);

#endif