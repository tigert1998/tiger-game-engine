#include "frame_buffer_object.h"

#include <glad/glad.h>

#include "utils.h"

FrameBufferObject::FrameBufferObject(uint32_t width, uint32_t height,
                                     bool color) {
  glGenFramebuffers(1, &id_);
  glBindFramebuffer(GL_FRAMEBUFFER, id_);

  if (color) {
    color_texture_ = Texture(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
                             GL_REPEAT, GL_LINEAR, GL_LINEAR, {}, false);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_texture_.id(), 0);
  } else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }

  depth_texture_ =
      Texture(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT,
              GL_REPEAT, GL_LINEAR, GL_LINEAR, {}, false);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depth_texture_.id(), 0);
  CheckOpenGLError();
  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBufferObject::FrameBufferObject(uint32_t width, uint32_t height,
                                     uint32_t depth) {
  // now for cascaded shadow mapping
  glGenFramebuffers(1, &id_);
  glBindFramebuffer(GL_FRAMEBUFFER, id_);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  depth_texture_ =
      Texture(GL_TEXTURE_2D_ARRAY, width, height, depth, GL_DEPTH_COMPONENT,
              GL_DEPTH_COMPONENT, GL_FLOAT, GL_CLAMP_TO_BORDER, GL_NEAREST,
              GL_NEAREST, {1, 1, 1, 1}, false);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture_.id(),
                       0);
  CheckOpenGLError();
  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBufferObject::~FrameBufferObject() {
  color_texture_.Clear();
  depth_texture_.Clear();
  glDeleteFramebuffers(1, &id_);
}

void FrameBufferObject::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id_); }

void FrameBufferObject::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }