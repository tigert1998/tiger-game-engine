#ifndef BLOOM_H_
#define BLOOM_H_

#include <stdint.h>

#include <glm/glm.hpp>
#include <vector>

#include "frame_buffer_object.h"
#include "post_processes.h"
#include "shader.h"
#include "texture.h"

class Bloom : public PostProcess {
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
  float filter_radius_ = 0.005f;

  void CreateMipChain();
  void RenderDownsamples();
  void RenderUpsamples();

 public:
  Bloom(uint32_t width, uint32_t height, uint32_t mip_chain_length);

  void Resize(uint32_t width, uint32_t height) override;

  const FrameBufferObject* fbo() const override { return input_fbo_.get(); }

  void Draw(const FrameBufferObject* dest_fbo) override;

  ~Bloom();

  void ImGuiWindow() override;
};

#endif