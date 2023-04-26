#ifndef SHADER_H_
#define SHADER_H_

#include <string>
#include <vector>

class Shader {
 public:
  Shader(const std::string &vs, const std::string &fs);
  Shader(const std::vector<std::pair<uint32_t, std::string>> &pairs);
  void Use() const;
  template <typename T>
  void SetUniform(const std::string &identifier, const T &) const;
  template <typename T>
  T GetUniform(const std::string &identifier) const;
  std::vector<std::string> GetUniformVariableVector() const;

 private:
  static uint32_t Compile(uint32_t type, const std::string &source,
                          const std::string &path);
  static uint32_t Link(const std::vector<uint32_t> &ids);

  uint32_t id_;
};

#endif