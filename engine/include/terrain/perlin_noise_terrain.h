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
  int size_, subdiv_size_;
  double ratio_;
  glm::mat4 transform_;

 public:
  PerlinNoiseTerrain(int size, int subdiv_size, double ratio,
                     glm::mat4 transform);

  double get_height(double x, double y);

  void ExportToOBJ(const std::string &file_path);
};

#endif