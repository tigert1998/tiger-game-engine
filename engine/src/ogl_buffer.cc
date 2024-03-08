#include "ogl_buffer.h"

OGLBuffer::OGLBuffer(uint32_t target, uint32_t size, void* data, uint32_t usage,
                     int32_t index)
    : target_(target), index_(index), has_ownership_(true) {
  glGenBuffers(1, &id_);
  Bind();
  glBufferData(target_, size, data, usage);
  if (index >= 0) BindBufferBase();
  Unbind();
}

OGLBuffer::OGLBuffer(uint32_t target, uint32_t id, int32_t index,
                     bool take_ownership)
    : target_(target), id_(id), index_(index), has_ownership_(take_ownership) {
  if (index >= 0) BindBufferBase();
}

OGLBuffer::~OGLBuffer() {
  if (has_ownership_) glDeleteBuffers(1, &id_);
}

void OGLBuffer::SubData(uint32_t offset, uint32_t size, const void* data) {
  Bind();
  glBufferSubData(target_, offset, size, data);
  Unbind();
}