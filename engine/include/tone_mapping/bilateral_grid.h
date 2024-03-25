#ifndef TONE_MAPPING_BILATERAL_GRID_H_
#define TONE_MAPPING_BILATERAL_GRID_H_

#include "post_processes.h"

namespace tone_mapping {

class BilateralGrid : public PostProcess {
 private:
  static std::vector<std::unique_ptr<Shader>> kPasses;

  std::unique_ptr<FrameBufferObject> input_fbo_;
  Texture weights_, grids_[2];
  uint32_t vao_;
  uint32_t width_, height_, grid_width_, grid_height_, grid_depth_;

  int32_t scale_size_, scale_range_;
  float alpha_, beta_, blend_, exposure_, sigma_size_, sigma_range_;

  void set_sigma(float sigma_size, float sigma_range);

 public:
  BilateralGrid(uint32_t width, uint32_t height);
  ~BilateralGrid();
  void Resize(uint32_t width, uint32_t height) override;
  const FrameBufferObject *fbo() const override;
  void Draw(const FrameBufferObject *dest_fbo) override;
  void ImGuiWindow() override;
};

}  // namespace tone_mapping
#endif