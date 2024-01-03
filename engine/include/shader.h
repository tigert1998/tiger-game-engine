#ifndef SHADER_H_
#define SHADER_H_

#include <any>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "texture.h"

class Shader {
 public:
  Shader(const std::string &vs, const std::string &fs,
         const std::map<std::string, std::any> &compile_time_constants);
  Shader(const std::vector<std::pair<uint32_t, std::string>> &pairs,
         const std::map<std::string, std::any> &compile_time_constants);
  void Use() const;
  template <typename T>
  void SetUniform(const std::string &identifier, const T &) const;
  void SetUniformSampler(const std::string &identifier, const Texture &texture,
                         uint32_t unit) const;
  template <typename T>
  T GetUniform(const std::string &identifier) const;
  std::vector<std::string> GetUniformVariables() const;
  bool UniformVariableExists(const std::string &identifier) const;

 private:
  static uint32_t Compile(uint32_t type, const std::string &source,
                          const std::string &path);
  static uint32_t Link(const std::vector<uint32_t> &ids);
  static std::string InsertCompileTimeConstants(
      const std::string &source,
      const std::map<std::string, std::any> &compile_time_constants);

  uint32_t id_;
};

std::unique_ptr<Shader> ScreenSpaceShader(
    const std::string &fs,
    const std::map<std::string, std::any> &compile_time_constants);

#endif