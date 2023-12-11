#ifndef FRAME_BUFFER_OBJECT_H_
#define FRAME_BUFFER_OBJECT_H_

#include <stdint.h>

#include "texture.h"

class FrameBufferObject {
 private:
  uint32_t id_ = 0;
  Texture color_texture_;
  Texture depth_texture_;

 public:
  FrameBufferObject(uint32_t width, uint32_t height, bool color);
  FrameBufferObject(uint32_t width, uint32_t height, uint32_t depth);

  inline uint32_t id() const { return id_; }
  inline const Texture& color_texture() const { return color_texture_; }
  inline const Texture& depth_texture() const { return depth_texture_; }

  void Bind() const;
  void Unbind() const;

  ~FrameBufferObject();
};

#endif