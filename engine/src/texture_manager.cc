#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "texture_manager.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <map>
#include <string>
#include <vector>

#include "cg_exception.h"
#include "stb_image.h"

using std::map;
using std::string;
using std::vector;

uint32_t TextureManager::LoadTexture(const std::string& path) {
  return LoadTexture(path, GL_CLAMP_TO_BORDER);
}

uint32_t TextureManager::LoadTexture(const std::string& path, uint32_t wrap) {
  uint32_t texture;
  static map<string, uint32_t> memory;
  if (memory.count(path.c_str())) return memory[path.c_str()];
  LOG(INFO) << "loading texture at: \"" << path << "\"";
  int w, h, comp;
  stbi_set_flip_vertically_on_load(true);
  unsigned char* image = stbi_load(path.c_str(), &w, &h, &comp, 0);
  if (image == nullptr) throw LoadPictureError(path.c_str());

  glGenTextures(1, &texture);

  glBindTexture(GL_TEXTURE_2D, texture);

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

  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(image);
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
