#include "utils.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <cctype>
#include <vector>

std::string BaseName(const std::string &path) {
  int i;
  for (i = (int)path.length() - 1; i >= 0; i--)
    if (path[i] == '\\' || path[i] == '/') break;
  return path.substr(i + 1);
}

std::string ParentPath(const std::string &path) {
  int i;
  for (i = path.size() - 2; i >= 0; i--) {
    if (path[i] == '/' || path[i] == '\\' && path[i + 1] == '\\') {
      break;
    }
  }
  return path.substr(0, i);
}

std::string SnakeToPascal(const std::string &name) {
  std::vector<std::string> parts;
  int last = 0;
  for (int i = 0; i < name.size(); i++) {
    if (name[i] == '_') {
      parts.push_back(name.substr(last, i - last));
      last = i + 1;
    }
  }
  if (last < name.size()) {
    parts.push_back(name.substr(last, name.size() - last));
  }
  std::string ans = "";
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i].size() == 0) continue;
    ans.push_back(toupper(parts[i][0]));
    for (int j = 1; j < parts[i].size(); j++) {
      ans.push_back(tolower(parts[i][j]));
    }
  }
  return ans;
}

void CheckOpenGLError() {
  auto err = glGetError();

  if (err != GL_NO_ERROR) {
    std::string err_str;
    switch (err) {
      case GL_INVALID_ENUM:
        err_str = "GL_INVALID_ENUM";
      case GL_INVALID_VALUE:
        err_str = "GL_INVALID_VALUE";
      case GL_INVALID_OPERATION:
        err_str = "GL_INVALID_OPERATION";
      case GL_OUT_OF_MEMORY:
        err_str = "GL_OUT_OF_MEMORY";
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        err_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
      default:
        err_str = std::to_string(err);
    }
    LOG(FATAL) << err_str;
  }
}