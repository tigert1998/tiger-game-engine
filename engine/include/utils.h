#ifndef UTILS_H_
#define UTILS_H_

#include <assimp/matrix4x4.h>

#include <glm/glm.hpp>
#include <string>

template <typename T>
glm::mat4 Mat4FromAimatrix4x4(aiMatrix4x4t<T> matrix) {
  glm::mat4 res;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) res[j][i] = matrix[i][j];
  return res;
}

std::string BaseName(const std::string &path);

std::string ParentPath(const std::string &path);

std::string SnakeToPascal(const std::string &name);

void CheckOpenGLError();

#endif