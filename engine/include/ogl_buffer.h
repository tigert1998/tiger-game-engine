#ifndef OGL_BUFFER_H_
#define OGL_BUFFER_H_

#include <glad/glad.h>
#include <stdint.h>

#include <vector>

class OGLBuffer {
 public:
  template <typename T>
  inline explicit OGLBuffer(uint32_t target, const std::vector<T>& vec,
                            uint32_t usage, int32_t index)
      : OGLBuffer(target, vec.size() * sizeof(vec[0]), (void*)vec.data(), usage,
                  index) {}

  explicit OGLBuffer(uint32_t target, uint32_t size, void* data, uint32_t usage,
                     int32_t index);

  explicit OGLBuffer(uint32_t target, uint32_t id, int32_t index,
                     bool take_ownership);

  inline void Bind() { glBindBuffer(target_, id_); }

  inline void Unbind() { glBindBuffer(target_, 0); }

  inline void BindBufferBase() const { glBindBufferBase(target_, index_, id_); }

  inline void BindBufferBase(int32_t index) const {
    glBindBufferBase(target_, index, id_);
  }

  inline void* Map(uint32_t access) { return glMapBuffer(target_, access); }

  inline void Unmap() { glUnmapBuffer(target_); }

  void SubData(uint32_t offset, uint32_t size, const void* data);

  inline uint32_t id() const { return id_; }

  ~OGLBuffer();

 private:
  bool has_ownership_;
  uint32_t id_, target_;
  int32_t index_;
};

#endif