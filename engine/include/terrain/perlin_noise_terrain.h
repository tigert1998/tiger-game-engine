#ifndef TERRAIN_PERLIN_NOISE_TERRAIN_H_
#define TERRAIN_PERLIN_NOISE_TERRAIN_H_

#include <memory>

#include "camera.h"
#include "light_sources.h"
#include "random/perlin_noise.h"
#include "shader.h"

class PerlinNoiseTerrain {
 private:
  std::unique_ptr<PerlinNoise> perlin_noise_;
  int size_;
  double length_;
  double ratio_;
  uint32_t vao_, vbo_, ebo_, indices_size_, texture_id_;
  std::unique_ptr<Shader> shader_;

  glm::vec3 get_normal(double x, double y);

  static const std::string kVsSource, kFsSource;

 public:
  PerlinNoiseTerrain(int size, double length, double ratio,
                     const std::string &texture_path);

  double get_height(double x, double y);

  void Draw(Camera *camera_ptr, LightSources *light_sources,
            glm::mat4 model_matrix);
};

#endif