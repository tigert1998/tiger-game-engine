#ifndef OBB_H_
#define OBB_H_

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>

#include "aabb.h"
#include "camera.h"
#include "shader.h"

struct OBB {
  glm::vec3 min;
  alignas(16) glm::vec3 max;
  alignas(16) float rotation_data[12];

  inline OBB() = default;
  inline OBB(const AABB& aabb) {
    this->min = aabb.min;
    this->max = aabb.max;
    this->set_rotation(glm::mat3(1));
  }

  inline glm::vec3 center() const { return (max + min) * 0.5f; }
  inline glm::vec3 center_world_space() const { return rotation() * center(); }
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
  void Clear();
  void AppendOBBs(const std::vector<OBB>& obbs, std::optional<glm::vec3> color);
  void AllocateBuffer();
  void Draw(Camera* camera);

 private:
  struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::mat3 rotation;
  };

  uint32_t vao_, vbo_;
  std::vector<Vertex> vertices_;

  static std::unique_ptr<Shader> kShader;
  static const std::string kVsSource;
  static const std::string kFsSource;
};

#endif