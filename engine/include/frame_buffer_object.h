#ifndef FRAME_BUFFER_OBJECT_H_
#define FRAME_BUFFER_OBJECT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "texture.h"

class FrameBufferObject {
 private:
  uint32_t id_ = 0;
  std::vector<Texture> color_textures_;
  Texture depth_texture_;

  void AttachTexture(uint32_t attachment, const Texture &texture);

 public:
  inline uint32_t id() const { return id_; }

  FrameBufferObject(std::vector<Texture> &color_textures,
                    Texture &depth_texture);

  inline const Texture &color_texture(uint32_t i) const {
    return color_textures_.at(i);
  }
  inline const Texture &depth_texture() const { return depth_texture_; }

  void Bind() const;
  void Unbind() const;

  ~FrameBufferObject();
};

#endif