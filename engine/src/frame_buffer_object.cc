#include "frame_buffer_object.h"

#include <glad/glad.h>

#include "utils.h"

void FrameBufferObject::AttachTexture(uint32_t attachment,
                                      const Texture &texture) {
  if (texture.target() == GL_TEXTURE_2D_ARRAY) {
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture.id(), 0);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, texture.target(),
                           texture.id(), 0);
  }
}

void FrameBufferObject::AttachColorTextures(
    std::vector<Texture> &color_textures) {
  if (color_textures.size() >= 1) {
    color_textures_ = color_textures;

    std::vector<uint32_t> attachments;
    for (int i = 0; i < color_textures_.size(); i++) {
      attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
      AttachTexture(attachments.back(), color_textures_[i]);
    }
    glDrawBuffers(attachments.size(), attachments.data());
  } else {
    glDrawBuffer(GL_NONE);
  }
}

FrameBufferObject::FrameBufferObject(std::vector<Texture> &color_textures,
                                     Texture &depth_texture) {
  glGenFramebuffers(1, &id_);
  glBindFramebuffer(GL_FRAMEBUFFER, id_);

  glReadBuffer(GL_NONE);
  AttachColorTextures(color_textures);

  depth_texture_ = depth_texture;
  AttachTexture(GL_DEPTH_ATTACHMENT, depth_texture_.value());

  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBufferObject::FrameBufferObject(std::vector<Texture> &color_textures) {
  glGenFramebuffers(1, &id_);
  glBindFramebuffer(GL_FRAMEBUFFER, id_);

  glReadBuffer(GL_NONE);
  AttachColorTextures(color_textures);

  CHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

FrameBufferObject::~FrameBufferObject() { glDeleteFramebuffers(1, &id_); }

void FrameBufferObject::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id_); }

void FrameBufferObject::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }