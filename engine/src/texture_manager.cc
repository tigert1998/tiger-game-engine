#include "texture_manager.h"

#include <SOIL2.h>
#include <glad/glad.h>
#include <glog/logging.h>

#include <gli/gli.hpp>
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
  static std::map<string, uint32_t> memory;
  if (memory.count(path.c_str())) return memory[path.c_str()];
  LOG(INFO) << "loading texture at: \"" << path << "\"";

  uint32_t texture;

  if (ToLower(path.substr(path.size() - 4)) == ".dds") {
    // We do not flip DDS image since it's not supported!
    gli::texture gli_texture = gli::load(path);
    if (gli_texture.empty()) throw LoadPictureError(path, "");

    gli::gl gl(gli::gl::PROFILE_GL33);
    gli::gl::format format =
        gl.translate(gli_texture.format(), gli_texture.swizzles());
    auto target = gl.translate(gli_texture.target());
    CHECK(gli::is_compressed(gli_texture.format()) && target == GL_TEXTURE_2D);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(gli_texture.levels() - 1));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA,
                     &format.Swizzles[0]);
    glTexStorage2D(GL_TEXTURE_2D, static_cast<GLint>(gli_texture.levels()),
                   format.Internal, gli_texture.extent(0).x,
                   gli_texture.extent(0).y);
    for (auto level = 0; level < gli_texture.levels(); ++level) {
      auto extent = gli_texture.extent(level);
      glCompressedTexSubImage2D(GL_TEXTURE_2D, static_cast<int32_t>(level), 0,
                                0, extent.x, extent.y, format.Internal,
                                static_cast<int32_t>(gli_texture.size(level)),
                                gli_texture.data(0, 0, level));
    }
  } else {
    texture = SOIL_load_OGL_texture(path.c_str(), 0, 0, SOIL_FLAG_INVERT_Y);
    if (texture == 0) throw LoadPictureError(path, "");
  }

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
