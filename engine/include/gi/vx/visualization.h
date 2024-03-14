#ifndef GI_VX_VISUALIZATION_H_
#define GI_VX_VISUALIZATION_H_

#include "camera.h"
#include "frame_buffer_object.h"
#include "shader.h"

namespace vxgi {

class Visualization {
 private:
  static std::unique_ptr<Shader> kCubeShader;
  static std::unique_ptr<Shader> kVisualizationShader;

  uint32_t vao_, vbo_, width_, height_;
  std::unique_ptr<FrameBufferObject> front_fbo_, back_fbo_;

  void DrawCubeCoord(const Camera* camera, const FrameBufferObject* fbo,
                     uint32_t cull_face, float world_size);

 public:
  explicit Visualization(uint32_t width, uint32_t height);
  ~Visualization();

  void Draw(const Camera* camera, const Texture& voxel, uint32_t voxel_type,
            float world_size, uint32_t mipmap_level);

  void Resize(uint32_t width, uint32_t height);
};

}  // namespace vxgi

#endif