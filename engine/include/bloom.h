#ifndef BLOOM_H_
#define BLOOM_H_

#include <stdint.h>

#include <glm/glm.hpp>
#include <vector>

#include "frame_buffer_object.h"
#include "shader.h"
#include "texture.h"

class Bloom {
 private:
  static std::unique_ptr<Shader> kDownsampleShader, kUpsampleShader,
      kRenderShader;

  struct BloomMip {
    glm::ivec2 size;
    std::unique_ptr<FrameBufferObject> fbo;
  };

  std::vector<BloomMip> mip_chain_;
  std::unique_ptr<FrameBufferObject> input_fbo_;

  uint32_t width_, height_, mip_chain_length_;
  uint32_t vao_;
  float filter_radius_ = 0.005, exposure_ = 1;

  void CreateMipChain();
  void RenderDownsamples();
  void RenderUpsamples();

 public:
  Bloom(uint32_t width, uint32_t height, uint32_t mip_chain_length);

  void Resize(uint32_t width, uint32_t height);

  const FrameBufferObject* fbo() const { return input_fbo_.get(); }

  void Draw(const FrameBufferObject* dest_fbo);

  ~Bloom();

  void ImGuiWindow();
};

#endif