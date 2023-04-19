#ifndef TEXTURE_MANAGER_H_
#define TEXTURE_MANAGER_H_

#include <stdint.h>

#include <string>

class TextureManager {
 public:
  static uint32_t LoadTexture(const std::string &path);

  static uint32_t LoadTexture(const std::string &path, uint32_t wrap);

  static uint32_t AllocateTexture(uint32_t height, uint32_t width,
                                  uint32_t format);

  static uint32_t AllocateRenderBuffer(uint32_t height, uint32_t width,
                                       uint32_t format);
};

#endif