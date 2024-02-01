#ifndef SSBO_H_
#define SSBO_H_

#include <glad/glad.h>
#include <stdint.h>

#include <vector>

class SSBO {
 public:
  template <typename T>
  inline explicit SSBO(const std::vector<T>& vec, uint32_t usage,
                       uint32_t index)
      : SSBO(vec.size() * sizeof(vec[0]), (void*)vec.data(), usage, index) {}

  inline explicit SSBO(uint32_t size, void* data, uint32_t usage,
                       uint32_t index)
      : index_(index), has_ownership_(true) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
    BindBufferBase();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  inline explicit SSBO(uint32_t id, uint32_t index, bool take_ownership)
      : id_(id), index_(index), has_ownership_(take_ownership) {
    BindBufferBase();
  }

  inline void BindBufferBase() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index_, id_);
  }

  inline void BindBufferBase(uint32_t index) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, id_);
  }

  inline uint32_t id() const { return id_; }

  inline ~SSBO() {
    if (has_ownership_) glDeleteBuffers(1, &id_);
  }

 private:
  bool has_ownership_;
  uint32_t id_, index_;
};

#endif