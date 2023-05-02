#ifndef GRASS_GRASS_H_
#define GRASS_GRASS_H_

#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>

#include "blade.h"
#include "bvh.h"
#include "camera.h"
#include "light_sources.h"
#include "shader.h"

class Grassland {
 public:
  Grassland(const std::string &terrain_model_path);

  void Draw(Camera *camera, LightSources *light_sources);

  ~Grassland();

 private:
  struct VertexType {
    glm::vec3 position;
  };

  uint32_t vbo_;
  std::vector<VertexType> vertices_for_bvh_;
  std::vector<glm::uvec3> triangles_for_bvh_;

  static const std::string kCsSource;

  std::unique_ptr<Blade> blade_;

  std::unique_ptr<BVH<VertexType>> bvh_;
  std::map<BVHNode *, std::vector<glm::mat4>> blade_transforms_;
  std::vector<glm::mat4> blade_transforms_for_gpu_;
};

#endif