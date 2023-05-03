#include <glm/gtc/matrix_transform.hpp>

#include "terrain/perlin_noise_terrain_generator.h"

int main() {
  glm::mat4 transform =
      glm::translate(glm::scale(glm::mat4(1), glm::vec3(128, 32, 128)),
                     glm::vec3(-0.5, 0, -0.5));
  auto terrain_generator =
      std::make_unique<PerlinNoiseTerrainGenerator>(256, 256, 0.5, transform);
  terrain_generator->ExportToOBJ("resources/terrain/sample.obj");
}