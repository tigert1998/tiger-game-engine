#include "shader.h"

#include <glad/glad.h>

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

#include "cg_exception.h"

Shader::Shader(const std::string &vs, const std::string &fs)
    : Shader(std::vector<std::pair<uint32_t, std::string>>{
          {GL_VERTEX_SHADER, vs}, {GL_FRAGMENT_SHADER, fs}}) {}

Shader::Shader(const std::vector<std::pair<uint32_t, std::string>> &pairs) {
  std::vector<uint32_t> ids;
  for (int i = 0; i < pairs.size(); i++) {
    ids.push_back(Compile(pairs[i].first, pairs[i].second, ""));
  }
  id_ = Link(ids);
}

uint32_t Shader::Compile(uint32_t type, const std::string &source,
                         const std::string &path) {
  uint32_t shader_id = glCreateShader(type);
  const char *temp = source.c_str();
  glShaderSource(shader_id, 1, &temp, nullptr);
  glCompileShader(shader_id);
  int success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
  if (success) return shader_id;
  int length;
  glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
  char *log = new char[length];
  glGetShaderInfoLog(shader_id, length, nullptr, log);
  std::string log_str = log;
  delete[] log;
  throw ShaderCompileError(path.c_str(), log_str);
}

uint32_t Shader::Link(const std::vector<uint32_t> &ids) {
  uint32_t program_id = glCreateProgram();

  for (auto id : ids) glAttachShader(program_id, id);
  glLinkProgram(program_id);
  for (auto id : ids) glDeleteShader(id);

  int success;
  glGetProgramiv(program_id, GL_LINK_STATUS, &success);
  if (success) return program_id;
  int length;
  glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
  char *log = new char[length];
  glGetProgramInfoLog(program_id, length, nullptr, log);
  std::string log_str = log;
  delete[] log;
  throw ShaderLinkError(log_str);
}

void Shader::Use() const { glUseProgram(id_); }

template <>
void Shader::SetUniform<glm::vec3>(const std::string &identifier,
                                   const glm::vec3 &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniform3fv(location, 1, glm::value_ptr(value));
}

template <>
void Shader::SetUniform<glm::vec4>(const std::string &identifier,
                                   const glm::vec4 &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniform4fv(location, 1, glm::value_ptr(value));
}

template <>
void Shader::SetUniform<glm::mat4>(const std::string &identifier,
                                   const glm::mat4 &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

template <>
void Shader::SetUniform<int32_t>(const std::string &identifier,
                                 const int32_t &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniform1i(location, value);
}

template <>
void Shader::SetUniform<uint32_t>(const std::string &identifier,
                                  const uint32_t &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniform1ui(location, value);
}

template <>
int32_t Shader::GetUniform(const std::string &identifier) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  int32_t value;
  glGetUniformiv(id_, location, &value);
  return value;
}

template <>
void Shader::SetUniform<float>(const std::string &identifier,
                               const float &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  glUniform1f(location, value);
}

template <>
void Shader::SetUniform<std::vector<glm::mat4>>(
    const std::string &identifier, const std::vector<glm::mat4> &value) const {
  auto location = glGetUniformLocation(id_, identifier.c_str());
  if (location < 0)
    throw ShaderSettingError(identifier, GetUniformVariableVector());
  if (value.size() == 0) return;
  glUniformMatrix4fv(location, value.size(), GL_FALSE,
                     glm::value_ptr(value[0]));
}

std::vector<std::string> Shader::GetUniformVariableVector() const {
  const int32_t buf_size = 32;
  char name[buf_size];
  int32_t length;
  int32_t size;
  uint32_t type;
  std::vector<std::string> names;

  int32_t count;
  glGetProgramiv(id_, GL_ACTIVE_UNIFORMS, &count);
  for (int i = 0; i < count; i++) {
    glGetActiveUniform(id_, i, buf_size, &length, &size, &type, name);
    names.push_back(std::string(name, name + length));
  }

  return names;
}