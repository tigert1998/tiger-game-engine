#include "texture.h"

#include <SOIL2.h>
#include <glad/glad.h>
#include <stb_image.h>

#include <filesystem>
#include <gli/gli.hpp>
#include <map>

#include "cg_exception.h"
#include "utils.h"

namespace fs = std::filesystem;

Texture Texture::Reference() const {
  Texture texture;
  texture.id_ = id_;
  texture.target_ = target_;
  texture.has_ownership_ = false;
  return texture;
}

void Texture::Load2DTextureFromPath(const fs::path &path, uint32_t wrap,
                                    uint32_t min_filter, uint32_t mag_filter,
                                    const std::vector<float> &border_color,
                                    bool mipmap, bool flip_y, bool srgb) {
  target_ = GL_TEXTURE_2D;

  if (ToLower(path.extension().string()) == ".dds") {
    gli::texture gli_texture = gli::load_dds(path.string());
    if (gli_texture.empty()) throw LoadPictureError(path, "");

    if (flip_y) gli_texture = gli::flip(gli_texture);
    gli::gl gl(gli::gl::PROFILE_GL33);
    gli::gl::format format =
        gl.translate(gli_texture.format(), gli_texture.swizzles());
    auto target = gl.translate(gli_texture.target());
    bool compressed = gli::is_compressed(gli_texture.format());
    if (target != GL_TEXTURE_2D) {
      fmt::print(stderr, "[error] target != GL_TEXTURE_2D\n");
      exit(1);
    }

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                    static_cast<int32_t>(gli_texture.levels() - 1));
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA,
                     &format.Swizzles[0]);
    glTexStorage2D(GL_TEXTURE_2D, static_cast<int32_t>(gli_texture.levels()),
                   format.Internal, gli_texture.extent(0).x,
                   gli_texture.extent(0).y);
    for (auto level = 0; level < gli_texture.levels(); ++level) {
      auto extent = gli_texture.extent(level);
      if (compressed) {
        glCompressedTexSubImage2D(GL_TEXTURE_2D, static_cast<int32_t>(level), 0,
                                  0, extent.x, extent.y, format.Internal,
                                  static_cast<int32_t>(gli_texture.size(level)),
                                  gli_texture.data(0, 0, level));
      } else {
        glTexSubImage2D(GL_TEXTURE_2D, static_cast<int32_t>(level), 0, 0,
                        extent.x, extent.y, format.External, format.Type,
                        gli_texture.data(0, 0, level));
      }
    }
  } else {
    stbi_set_flip_vertically_on_load(flip_y);
    int32_t width, height, comp;
    uint8_t *image =
        stbi_load(path.string().c_str(), &width, &height, &comp, 0);
    if (image == nullptr) throw LoadPictureError(path, "");

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    if (comp == 1) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED,
                   GL_UNSIGNED_BYTE, image);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    } else if (comp == 3) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      uint32_t internal_format = srgb ? GL_SRGB_ALPHA : GL_RGBA;
      glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGB,
                   GL_UNSIGNED_BYTE, image);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    } else if (comp == 4) {
      uint32_t internal_format = srgb ? GL_SRGB_ALPHA : GL_RGBA;
      glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, image);
    }
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  if (wrap == GL_CLAMP_TO_BORDER) {
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                     border_color.data());
  }

  if (mipmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::LoadCubeMapTextureFromPath(const fs::path &path, uint32_t wrap,
                                         uint32_t min_filter,
                                         uint32_t mag_filter,
                                         const std::vector<float> &border_color,
                                         bool mipmap, bool flip_y, bool srgb) {
  if (flip_y) {
    fmt::print(stderr, "[error] CubeMap does not support flipping\n");
    exit(1);
  }

  const static std::map<std::string, uint32_t> kTypes = {
      {"posx", GL_TEXTURE_CUBE_MAP_POSITIVE_X},
      {"negx", GL_TEXTURE_CUBE_MAP_NEGATIVE_X},
      {"posy", GL_TEXTURE_CUBE_MAP_POSITIVE_Y},
      {"negy", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y},
      {"posz", GL_TEXTURE_CUBE_MAP_POSITIVE_Z},
      {"negz", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z},
  };

  target_ = GL_TEXTURE_CUBE_MAP;
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, id_);

  std::map<std::string, uint32_t> exists;
  for (auto kv : kTypes) exists[kv.first] = 0;

  for (const auto &entry : fs::directory_iterator(path)) {
    std::string stem = entry.path().stem().string();
    if (exists.count(stem) && exists[stem] == 0) {
      int w, h, comp;
      // Cubemap follows RenderMan's convention:
      // https://stackoverflow.com/questions/11685608/convention-of-faces-in-opengl-cubemapping/
      stbi_set_flip_vertically_on_load(false);
      uint8_t *image =
          SOIL_load_image(entry.path().string().c_str(), &w, &h, &comp, 0);
      if (image == nullptr) throw LoadPictureError(entry.path(), "");

      uint32_t internal_format = srgb ? GL_SRGB : GL_RGB;
      if (comp == 3) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(kTypes.at(stem), 0, internal_format, w, h, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, image);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      } else if (comp == 4) {
        glTexImage2D(kTypes.at(stem), 0, internal_format, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image);
      }

      SOIL_free_image_data(image);

      exists[stem] = 1;
    }
  }

  for (auto kv : kTypes)
    if (!exists[kv.first]) {
      throw LoadPictureError(path, "");
    }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, mag_filter);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap);

  if (wrap == GL_CLAMP_TO_BORDER) {
    glTexParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR,
                     border_color.data());
  }

  if (mipmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }
}

Texture::Texture(const fs::path &path, uint32_t wrap, uint32_t min_filter,
                 uint32_t mag_filter, const std::vector<float> &border_color,
                 bool mipmap, bool flip_y, bool srgb) {
  has_ownership_ = true;
  if (fs::is_directory(fs::path(path))) {
    LoadCubeMapTextureFromPath(path, wrap, min_filter, mag_filter, border_color,
                               mipmap, flip_y, srgb);
  } else {
    Load2DTextureFromPath(path, wrap, min_filter, mag_filter, border_color,
                          mipmap, flip_y, srgb);
  }
}

Texture Texture::LoadFromFS(const fs::path &path, uint32_t wrap,
                            uint32_t min_filter, uint32_t mag_filter,
                            const std::vector<float> &border_color, bool mipmap,
                            bool flip_y, bool srgb) {
  static std::map<fs::path, Texture> textures;

  if (textures.count(path)) return textures[path];
  Texture texture(path, wrap, min_filter, mag_filter, border_color, mipmap,
                  flip_y, srgb);
  textures[path] = texture.Reference();
  return texture;
}

Texture::Texture(void *data, uint32_t width, uint32_t height,
                 uint32_t internal_format, uint32_t format, uint32_t type,
                 uint32_t wrap, uint32_t min_filter, uint32_t mag_filter,
                 const std::vector<float> &border_color, bool mipmap) {
  has_ownership_ = true;
  target_ = GL_TEXTURE_2D;

  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
               type, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

  if (wrap == GL_CLAMP_TO_BORDER) {
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                     border_color.data());
  }

  if (mipmap) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(void *data, uint32_t target, uint32_t width, uint32_t height,
                 uint32_t depth, uint32_t internal_format, uint32_t format,
                 uint32_t type, uint32_t wrap, uint32_t min_filter,
                 uint32_t mag_filter, const std::vector<float> &border_color,
                 bool mipmap) {
  has_ownership_ = true;
  if (target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY) {
    fmt::print(
        stderr,
        "[error] target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY\n");
    exit(1);
  }
  target_ = target;

  glGenTextures(1, &id_);
  glBindTexture(target, id_);

  glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
  if (target == GL_TEXTURE_3D) {
    glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap);
  }
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
  glTexImage3D(target, 0, internal_format, width, height, depth, 0, format,
               type, data);

  if (wrap == GL_CLAMP_TO_BORDER) {
    glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, border_color.data());
  }

  if (mipmap) {
    glGenerateMipmap(target);
  }

  glBindTexture(target, 0);
}

Texture::Texture(const std::vector<void *> &data, uint32_t width,
                 uint32_t height, uint32_t internal_format, uint32_t format,
                 uint32_t type, uint32_t wrap, uint32_t min_filter,
                 uint32_t mag_filter, const std::vector<float> &border_color,
                 bool mipmap) {
  std::vector<void *> data_copyed = data;
  if (data_copyed.size() == 0) data_copyed.resize(6, nullptr);

  has_ownership_ = true;

  target_ = GL_TEXTURE_CUBE_MAP;

  glGenTextures(1, &id_);
  glBindTexture(target_, id_);

  glTexParameteri(target_, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(target_, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(target_, GL_TEXTURE_WRAP_R, wrap);
  glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, mag_filter);

  for (int i = 0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, width,
                 height, 0, format, type, data_copyed[i]);
  }

  if (wrap == GL_CLAMP_TO_BORDER) {
    glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, border_color.data());
  }

  if (mipmap) {
    glGenerateMipmap(target_);
  }

  glBindTexture(target_, 0);
}

void Texture::Clear() {
  if (has_ownership_ && id_ != 0) {
    glDeleteTextures(1, &id_);
  }
  id_ = 0;
  target_ = 0;
  has_ownership_ = false;
}

Texture::~Texture() { Clear(); }

uint64_t Texture::handle() const {
  if (!handle_.has_value()) {
    handle_ = {glGetTextureHandleARB(id_)};
  }
  return handle_.value();
}

void Texture::MakeResident() const { glMakeTextureHandleResidentARB(handle()); }

void Texture::MakeNonResident() const {
  glMakeTextureHandleNonResidentARB(handle());
}

Texture Texture::Empty(uint32_t target) {
  Texture empty;
  empty.target_ = target;
  return empty;
}