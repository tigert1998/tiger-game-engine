#include "terrain/perlin_noise_terrain.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <vector>

#include "texture_manager.h"
#include "vertex.h"

PerlinNoiseTerrain::PerlinNoiseTerrain(int size, int subdiv_size, double ratio,
                                       glm::mat4 transform)
    : size_(size),
      subdiv_size_(subdiv_size),
      ratio_(ratio),
      transform_(transform) {
  CHECK(size_ % subdiv_size_ == 0);

  // size * size squares
  perlin_noise_.reset(new PerlinNoise(1024, 10086));
}

double PerlinNoiseTerrain::get_height(double x, double y) {
  return perlin_noise_->Noise(x * ratio_, y * ratio_, 0);
}

void PerlinNoiseTerrain::ExportToOBJ(const std::string &file_path) {
  int32_t num_subdivs = size_ / subdiv_size_;

  auto fp = fopen(file_path.c_str(), "w");

  int32_t num_vertices = 0;
  for (int i = 0; i < num_subdivs; i++)
    for (int j = 0; j < num_subdivs; j++) {
      fprintf(fp, "o mesh_%d_%d\n", i, j);
      std::vector<glm::vec3> v;

      for (int k = i * subdiv_size_; k <= (i + 1) * subdiv_size_; k++)
        for (int l = j * subdiv_size_; l <= (j + 1) * subdiv_size_; l++) {
          double x = 1.0 * k / size_;
          double z = 1.0 * l / size_;
          double y = get_height(x, z);
          v.emplace_back(glm::vec3(transform_ * glm::vec4(x, y, z, 1)));
        }

      for (auto vec : v) {
        fprintf(fp, "v %.6f %.6f %.6f\n", vec.x, vec.y, vec.z);
      }

      for (int k = 0; k <= subdiv_size_; k++)
        for (int l = 0; l <= subdiv_size_; l++) {
          int a = k * (subdiv_size_ + 1) + l;
          int b = k * (subdiv_size_ + 1) + l + 1;
          int c = (k + 1) * (subdiv_size_ + 1) + l;
          int d = (k + 1) * (subdiv_size_ + 1) + l + 1;

          a += num_vertices + 1;
          b += num_vertices + 1;
          c += num_vertices + 1;
          d += num_vertices + 1;

          fprintf(fp, "f %d %d %d\n", a, b, c);
          fprintf(fp, "f %d %d %d\n", c, b, d);
        }

      num_vertices += v.size();
    }

  fclose(fp);
}