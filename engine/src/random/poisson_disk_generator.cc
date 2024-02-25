#include "random/poisson_disk_generator.h"

#include <glm/gtc/constants.hpp>

PoissonDiskGenerator::PoissonDiskGenerator() {}

std::vector<glm::vec2> PoissonDiskGenerator::Generate2D(uint32_t num_points,
                                                        uint32_t num_grids) {
  std::vector<glm::vec2> samples_2d;
  std::vector<int32_t> grid;
  float radius = 1.0 / num_grids * sqrt(2);

  auto set_grid = [num_grids, &grid](glm::vec2 coord, int32_t samples_index) {
    auto grid_coord = glm::ivec2(coord * glm::vec2(num_grids));
    grid[grid_coord.x * num_grids + grid_coord.y] = samples_index;
  };

  auto check_collision = [num_grids, radius, &grid,
                          &samples_2d](glm::vec2 coord) -> bool {
    auto grid_coord = glm::ivec2(coord * glm::vec2(num_grids));
    for (int i = std::max(0, grid_coord.x - 2);
         i <= std::min(grid_coord.x + 2, (int32_t)num_grids - 1); i++)
      for (int j = std::max(0, grid_coord.y - 2);
           j <= std::min(grid_coord.y + 2, (int32_t)num_grids - 1); j++) {
        if (int32_t samples_index = grid[i * num_grids + j]; samples_index >= 0)
          if (glm::distance(samples_2d[samples_index], coord) < radius)
            return true;
      }
    return false;
  };

  auto dis = std::uniform_real_distribution<float>(0, 1);

  samples_2d.resize(1, glm::vec2(0.5, 0.5));
  grid.resize(num_grids * num_grids, -1);
  set_grid(samples_2d[0], 0);

  uint32_t head = 0;
  while (head < samples_2d.size() && head < num_points) {
    glm::vec2 coord = samples_2d[head];
    head += 1;

    for (int i = 0; i < 100; i++) {
      float theta = dis(engine_) * 2 * glm::pi<float>();
      glm::vec2 offset =
          glm::vec2(cos(theta), sin(theta)) * (1 + dis(engine_)) * radius;
      glm::vec2 new_coord = coord + offset;

      if (0 <= new_coord[0] && new_coord[0] < 1 && 0 <= new_coord[1] &&
          new_coord[1] < 1) {
        bool collision = check_collision(new_coord);
        if (!collision && samples_2d.size() < num_points) {
          samples_2d.push_back(new_coord);
          set_grid(new_coord, samples_2d.size() - 1);
        }
      }
    }
  }

  return samples_2d;
}