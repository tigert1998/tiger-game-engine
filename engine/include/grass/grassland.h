#ifndef GRASS_GRASS_H_
#define GRASS_GRASS_H_

#include <memory>
#include <string>

#include "blade.h"
#include "camera.h"
#include "light_sources.h"
#include "shader.h"

class Grassland {
 public:
  Grassland(const std::string &terrain_model_path);

  void Draw(Camera *camera, LightSources *light_sources);

  ~Grassland();

 private:
  uint32_t vertices_ssbo_, indices_ssbo_, blade_transforms_ssbo_,
      num_blades_buffer_, vbo_;
  uint32_t num_triangles_;

  static const std::string kCsSource;

  std::unique_ptr<Shader> calc_blade_transforms_shader_;
  std::unique_ptr<Blade> blade_;

  void CalcBladeTransforms();
};

#endif