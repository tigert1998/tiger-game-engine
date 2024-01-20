#ifndef SMAA_H_
#define SMAA_H_

#include <memory>
#include <string>

#include "frame_buffer_object.h"
#include "shader.h"

class SMAA {
 private:
  std::unique_ptr<FrameBufferObject> input_fbo_;
  std::unique_ptr<FrameBufferObject> edges_fbo_;
  std::unique_ptr<FrameBufferObject> blend_fbo_;

  const static std::string kCommonDefines;
  std::string smaa_lib_;

  std::string smaa_edge_detection_vs_source() const;
  std::string smaa_edge_detection_fs_source() const;
  std::string smaa_blending_weight_calc_vs_source() const;
  std::string smaa_blending_weight_calc_fs_source() const;
  std::string smaa_neighborhood_blending_vs_source() const;
  std::string smaa_neighborhood_blending_fs_source() const;

  std::unique_ptr<Shader> edge_detection_shader_;
  std::unique_ptr<Shader> blending_weight_calc_shader_;
  std::unique_ptr<Shader> neighborhood_blending_shader_;

  uint32_t width_, height_;
  uint32_t vao_, vbo_, ebo_;

  Texture area_, search_;

  void PrepareVertexData();
  void PrepareAreaAndSearchTexture(const std::string &smaa_repo_path);

 public:
  SMAA(const std::string &smaa_repo_path, uint32_t width, uint32_t height);

  void Resize(uint32_t width, uint32_t height);

  const FrameBufferObject *fbo() const { return input_fbo_.get(); }

  void Draw();

  ~SMAA();
};

#endif