#include "frame_buffer_object.h"

#include <glad/glad.h>

#include "texture_manager.h"
#include "utils.h"

FrameBufferObject::FrameBufferObject(uint32_t width, uint32_t height,
                                     bool color) {
  glGenFramebuffers(1, &id_);
  glBindFramebuffer(GL_FRAMEBUFFER, id_);
  if (color) {
    color_texture_id_ = TextureManager::AllocateTexture(
        width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_texture_id_, 0);
  } else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  depth_texture_id_ = TextureManager::AllocateTexture(
      width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, false);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depth_texture_id_, 0);
  CheckOpenGLError();
  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBufferObject::~FrameBufferObject() {
  if (color_texture_id_ != 0) {
    glDeleteTextures(1, &color_texture_id_);
  }
  if (depth_texture_id_ != 0) {
    glDeleteTextures(1, &depth_texture_id_);
  }
  glDeleteFramebuffers(1, &id_);
}

void FrameBufferObject::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, id_); }

void FrameBufferObject::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }