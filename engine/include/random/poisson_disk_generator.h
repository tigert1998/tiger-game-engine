#ifndef RANDOM_POISSON_DISK_GENERATOR_H_
#define RANDOM_POISSON_DISK_GENERATOR_H_

#include <glm/glm.hpp>
#include <random>
#include <vector>

class PoissonDiskGenerator {
 private:
  std::mt19937 engine_;

 public:
  explicit PoissonDiskGenerator();

  std::vector<glm::vec2> Generate2D(uint32_t num_points, uint32_t num_grids);

  std::vector<glm::vec3> Generate3D(uint32_t num_points, uint32_t num_grids);
};

#endif
