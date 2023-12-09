#define NOMINMAX

#include "texture_manager.h"

#include <SOIL2.h>
#include <glad/glad.h>
#include <glog/logging.h>
#include <stb_image.h>

#include <map>
#include <string>
#include <vector>

#include "cg_exception.h"
#include "utils.h"

using std::string;
using std::vector;

uint32_t TextureManager::LoadTexture(const std::string& path) {
  return LoadTexture(path, GL_CLAMP_TO_BORDER);
}

uint32_t TextureManager::LoadTexture(const std::string& path, uint32_t wrap) {
  uint32_t texture;
  static std::map<string, uint32_t> memory;
  if (memory.count(path.c_str())) return memory[path.c_str()];
  LOG(INFO) << "loading texture at: \"" << path << "\"";

  int w, h, comp;
  uint8_t* image;

  stbi_set_flip_vertically_on_load(true);
  image = SOIL_load_image(path.c_str(), &w, &h, &comp, SOIL_LOAD_AUTO);
  if (image == nullptr) throw LoadPictureError(path.c_str(), "");

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  if (comp == 1) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE,
                 image);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  } else if (comp == 3) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 image);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  } else if (comp == 4)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image);

  SOIL_free_image_data(image);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (wrap == GL_CLAMP_TO_BORDER) {
    vector<float> border_color(4);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                     border_color.data());
  }

  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);

  return memory[path.c_str()] = texture;
}

uint32_t TextureManager::AllocateTexture(uint32_t width, uint32_t height,
                                         uint32_t internal_format,
                                         uint32_t format, uint32_t type,
                                         bool mipmap) {
  uint32_t texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
               type, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (mipmap) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

uint32_t TextureManager::AllocateTexture3D(uint32_t width, uint32_t height,
                                           uint32_t depth,
                                           uint32_t internal_format,
                                           uint32_t format, uint32_t type,
                                           bool mipmap) {
  uint32_t texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_3D, texture);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_3D, 0, internal_format, width, height, depth, 0,
               format, type, nullptr);
  if (mipmap) {
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_3D);
  }
  glBindTexture(GL_TEXTURE_3D, 0);

  return texture;
}

uint32_t TextureManager::AllocateTexture2DArray(uint32_t width, uint32_t height,
                                                uint32_t depth,
                                                uint32_t internal_format,
                                                uint32_t format,
                                                uint32_t type) {
  uint32_t texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  constexpr float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_color);

  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internal_format, width, height, depth, 0,
               format, type, nullptr);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  return texture;
}
