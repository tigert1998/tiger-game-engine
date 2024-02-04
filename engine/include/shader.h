#ifndef SHADER_H_
#define SHADER_H_

#include <any>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "texture.h"

class Shader {
 public:
  Shader(const std::filesystem::path &vs, const std::filesystem::path &fs,
         const std::map<std::string, std::any> &defines);
  Shader(const std::vector<std::pair<uint32_t, std::filesystem::path>> &pairs,
         const std::map<std::string, std::any> &defines);
  void Use() const;
  template <typename T>
  void SetUniform(const std::string &identifier, const T &) const;
  void SetUniformSampler(const std::string &identifier, const Texture &texture,
                         uint32_t unit) const;
  template <typename T>
  T GetUniform(const std::string &identifier) const;
  std::vector<std::string> GetUniformVariables() const;
  bool UniformVariableExists(const std::string &identifier) const;
  static std::unique_ptr<Shader> ScreenSpaceShader(
      const std::filesystem::path &fs,
      const std::map<std::string, std::any> &defines);

  static std::vector<std::filesystem::path> include_directories;

 private:
  static std::optional<std::string> ReadFileInIncludeDirectory(
      const std::filesystem::path &path);
  static uint32_t Compile(uint32_t type, const std::filesystem::path &path,
                          const std::string &source);
  static uint32_t Link(const std::vector<uint32_t> &ids);
  static std::string InsertDefines(
      const std::filesystem::path &path, const std::string &source,
      const std::map<std::string, std::any> &defines);
  static std::string InsertIncludes(const std::filesystem::path &path,
                                    const std::string &source);

  uint32_t id_;
};

#endif