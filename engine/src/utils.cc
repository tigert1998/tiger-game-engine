#include "utils.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

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
        break;
      case GL_INVALID_VALUE:
        err_str = "GL_INVALID_VALUE";
        break;
      case GL_INVALID_OPERATION:
        err_str = "GL_INVALID_OPERATION";
        break;
      case GL_OUT_OF_MEMORY:
        err_str = "GL_OUT_OF_MEMORY";
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        err_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
        break;
      default:
        err_str = std::to_string(err);
    }
    LOG(FATAL) << err_str;
  }
}

std::string ReadFile(const std::string &file_path) {
  std::ifstream ifs(file_path);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));
  return content;
}

std::string ToLower(const std::string &str) {
  std::string ans;
  std::transform(str.begin(), str.end(), std::back_inserter(ans), std::tolower);
  return ans;
}