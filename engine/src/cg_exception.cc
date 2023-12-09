#include "cg_exception.h"

using std::string;

ShaderCompileError::ShaderCompileError(const string &title, const string &log) {
  error_message = "[shader compile error on \"" + title + "\"] " + log;
}

const char *ShaderCompileError::what() const noexcept {
  return error_message.c_str();
}

ShaderLinkError::ShaderLinkError(const string &log) {
  error_message = "[shader link error] " + log;
}

const char *ShaderLinkError::what() const noexcept {
  return error_message.c_str();
}

LoadPictureError::LoadPictureError(const std::string &path,
                                   const std::string &format) {
  error_message = "[picture format error] fail to load picture at \"" +
                  string(path.c_str()) + "\"";
  if (format != "") {
    error_message += " (format: " + format + ")";
  }
}

const char *LoadPictureError::what() const noexcept {
  return error_message.c_str();
}

AssimpError::AssimpError(const string &error_string) {
  error_message = "[assimp error] " + error_string;
}

const char *AssimpError::what() const noexcept { return error_message.c_str(); }

ShaderSettingError::ShaderSettingError(
    const std::string &name, const std::vector<std::string> &uniform_names) {
  error_message = "[shader setting error] fail to set uniform variable " +
                  name + ". Available uniform variables: {";
  for (int i = 0; i < uniform_names.size(); i++) {
    error_message += uniform_names[i];
    if (i < uniform_names.size() - 1) {
      error_message += ", ";
    }
  }
  error_message += "}";
}

const char *ShaderSettingError::what() const noexcept {
  return error_message.c_str();
}

MaxBoneExceededError::MaxBoneExceededError() = default;

const char *MaxBoneExceededError::what() const noexcept {
  return "[max bone exceeded error]";
}