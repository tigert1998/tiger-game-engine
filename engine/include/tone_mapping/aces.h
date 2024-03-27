#ifndef TONE_MAPPING_ACES_H_
#define TONE_MAPPING_ACES_H_

#include "post_processes.h"

namespace tone_mapping {

class ACES : public PostProcess {
 private:
  static std::unique_ptr<Shader> kShader;

  std::unique_ptr<FrameBufferObject> input_fbo_;
  uint32_t vao_;
  uint32_t width_, height_;
  float adapted_lum_ = 0.5;

 public:
  ACES(uint32_t width, uint32_t height);
  ~ACES();
  void Resize(uint32_t width, uint32_t height) override;
  const FrameBufferObject *fbo() const override;
  void Draw(const FrameBufferObject *dest_fbo) override;
  void ImGuiWindow() override;
};

}  // namespace tone_mapping

#endif