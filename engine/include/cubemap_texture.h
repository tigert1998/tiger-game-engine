#ifndef CUBEMAP_TEXTURE_H_
#define CUBEMAP_TEXTURE_H_

#include <string>
#include <vector>

class CubemapTexture {
 public:
  CubemapTexture(const std::string& path, const std::string& ext);

  ~CubemapTexture();

  void Bind(uint32_t unit);

 private:
  std::string path_, ext_;
  uint32_t id_;

  void Load();

  static const std::vector<std::pair<uint32_t, std::string>> kTypes;
};

#endif