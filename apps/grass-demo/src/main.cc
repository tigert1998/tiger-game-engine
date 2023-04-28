#include "terrain/perlin_noise_terrain.h"

int main() {
  auto terrain =
      std::make_unique<PerlinNoiseTerrain>(512, 16, 0.5, glm::mat4(1));
  terrain->ExportToOBJ("resources/terrain/sample.obj");
}