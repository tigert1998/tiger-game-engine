#ifndef FRAME_BUFFER_OBJECT_H_
#define FRAME_BUFFER_OBJECT_H_

#include <stdint.h>

class FrameBufferObject {
 private:
  uint32_t id_ = 0, color_texture_id_ = 0, depth_texture_id_ = 0;

 public:
  FrameBufferObject(uint32_t width, uint32_t height, bool color);
  FrameBufferObject(uint32_t width, uint32_t height, uint32_t depth);

  inline uint32_t id() { return id_; }
  inline uint32_t color_texture_id() { return color_texture_id_; }
  inline uint32_t depth_texture_id() { return depth_texture_id_; }

  void Bind();
  void Unbind();

  ~FrameBufferObject();
};

#endif