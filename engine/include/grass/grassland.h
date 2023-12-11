#ifndef GRASS_GRASS_H_
#define GRASS_GRASS_H_

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>

#include "blade.h"
#include "bvh.h"
#include "camera.h"
#include "light_sources.h"
#include "shader.h"
#include "texture.h"

class Grassland {
 public:
  Grassland(const std::string &terrain_model_path,
            const std::string &distortion_texture_path);

  void Draw(Camera *camera, LightSources *light_sources, double time);

  ~Grassland();

 private:
  struct VertexType {
    glm::vec3 position;
  };
  struct InstancingData {
    glm::mat4 transform;
    glm::vec3 position;
    glm::vec2 tex_coord;
  };

  uint32_t vbo_;
  std::vector<VertexType> vertices_for_bvh_;
  std::vector<glm::uvec3> triangles_for_bvh_;

  static const std::string kCsSource;

  std::unique_ptr<Blade> blade_;

  std::unique_ptr<BVH<VertexType>> bvh_;
  std::map<BVHNode *, std::vector<InstancingData>> blade_data_;
  std::vector<InstancingData> blade_data_for_gpu_;

  Texture distortion_texture_;

  static uint32_t CreateAndBindSSBO(uint32_t size, void *data, uint32_t usage,
                                    uint32_t index);
};

#endif