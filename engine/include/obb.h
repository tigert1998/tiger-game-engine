#ifndef OBB_H_
#define OBB_H_

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "camera.h"
#include "shader.h"

struct OBB {
  glm::vec3 min;
  alignas(16) glm::vec3 max;
  alignas(16) float rotation_data[12];

  inline glm::vec3 center() const { return (max + min) * 0.5f; }
  inline glm::vec3 extents() const { return max - center(); }
  inline glm::mat3 rotation() const {
    glm::mat3 ret;
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) ret[i][j] = rotation_data[i * 4 + j];
    return ret;
  }
  inline void set_rotation(glm::mat3 rotation) {
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) rotation_data[i * 4 + j] = rotation[i][j];
  }

  bool IntersectsOBB(const OBB& obb, float epsilon) const;

  static const std::string GLSLSource();
};

class OBBDrawer {
 public:
  explicit OBBDrawer();
  ~OBBDrawer();
  void CompileShaders();
  void Draw(Camera* camera, const std::vector<OBB>& obbs);

 private:
  struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::mat3 rotation;
  };

  uint32_t vao_, vbo_;

  static std::unique_ptr<Shader> kShader;
  static const std::string kVsSource;
  static const std::string kFsSource;
};

#endif