#ifndef SMAA_H_
#define SMAA_H_

#include <filesystem>
#include <memory>
#include <string>

#include "frame_buffer_object.h"
#include "post_processes.h"
#include "shader.h"

class SMAA : public PostProcess {
 private:
  std::unique_ptr<FrameBufferObject> input_fbo_;
  std::unique_ptr<FrameBufferObject> edges_fbo_;
  std::unique_ptr<FrameBufferObject> blend_fbo_;

  std::unique_ptr<Shader> edge_detection_shader_;
  std::unique_ptr<Shader> blending_weight_calc_shader_;
  std::unique_ptr<Shader> neighborhood_blending_shader_;

  uint32_t width_, height_;
  uint32_t vao_, vbo_, ebo_;

  Texture area_, search_;

  void AddShaderIncludeDirectory(const std::filesystem::path &smaa_repo_path);
  void PrepareVertexData();
  void PrepareAreaAndSearchTexture(const std::filesystem::path &smaa_repo_path);

 public:
  SMAA(const std::filesystem::path &smaa_repo_path, uint32_t width,
       uint32_t height);

  void Resize(uint32_t width, uint32_t height) override;

  const FrameBufferObject *fbo() const override { return input_fbo_.get(); }

  void Draw(const FrameBufferObject *dest_fbo) override;

  inline void ImGuiWindow() override {}

  ~SMAA();
};

#endif