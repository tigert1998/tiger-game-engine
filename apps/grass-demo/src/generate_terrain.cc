#include <glm/gtc/matrix_transform.hpp>

#include "terrain/perlin_noise_terrain.h"

int main() {
  glm::mat4 transform =
      glm::translate(glm::scale(glm::mat4(1), glm::vec3(128, 32, 128)),
                     glm::vec3(-0.5, 0, -0.5));
  auto terrain = std::make_unique<PerlinNoiseTerrain>(256, 256, 0.5, transform);
  terrain->ExportToOBJ("resources/terrain/sample.obj");
}