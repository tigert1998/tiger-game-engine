#ifndef SSBO_H_
#define SSBO_H_

#include <glad/glad.h>
#include <stdint.h>

class SSBO {
 public:
  inline explicit SSBO(uint32_t size, void* data, uint32_t usage,
                       uint32_t index)
      : index_(index) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
    BindBufferBase();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  inline void BindBufferBase() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index_, id_);
  }

  inline uint32_t id() const { return id_; }

  inline ~SSBO() { glDeleteBuffers(1, &id_); }

 private:
  uint32_t id_, index_;
};

#endif