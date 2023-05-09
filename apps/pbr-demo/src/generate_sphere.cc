#include <stdint.h>

#include <glm/glm.hpp>
#include <vector>

int main() {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> tex_coords;
  std::vector<glm::vec3> normals;
  std::vector<uint32_t> indices;

  const uint32_t X_SEGMENTS = 64;
  const uint32_t Y_SEGMENTS = 64;
  const float PI = 3.14159265359f;
  for (uint32_t x = 0; x <= X_SEGMENTS; ++x) {
    for (uint32_t y = 0; y <= Y_SEGMENTS; ++y) {
      float x_segment = (float)x / (float)X_SEGMENTS;
      float y_segment = (float)y / (float)Y_SEGMENTS;
      float x_pos = std::cos(x_segment * 2.0f * PI) * std::sin(y_segment * PI);
      float y_pos = std::cos(y_segment * PI);
      float z_pos = std::sin(x_segment * 2.0f * PI) * std::sin(y_segment * PI);

      positions.push_back(glm::vec3(x_pos, y_pos, z_pos));
      tex_coords.push_back(glm::vec2(x_segment, y_segment));
      normals.push_back(glm::vec3(x_pos, y_pos, z_pos));
    }
  }

  // GL_TRIANGLE_STRIP
  bool odd_row = false;
  for (uint32_t y = 0; y < Y_SEGMENTS; ++y) {
    if (!odd_row) {  // even rows: y == 0, y == 2; and so on
      for (uint32_t x = 0; x <= X_SEGMENTS; ++x) {
        indices.push_back(y * (X_SEGMENTS + 1) + x);
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
      }
    } else {
      for (int x = X_SEGMENTS; x >= 0; --x) {
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x);
      }
    }
    odd_row = !odd_row;
  }

  auto f = fopen("resources/sphere/sphere.obj", "w");
  for (const auto &v : positions) {
    fprintf(f, "v %.6f %.6f %.6f\n", v.x, v.y, v.z);
  }
  for (const auto &v : tex_coords) {
    fprintf(f, "vt %.6f %.6f\n", v.x, v.y);
  }
  for (const auto &v : normals) {
    fprintf(f, "vn %.6f %.6f %.6f\n", v.x, v.y, v.z);
  }
  for (int i = 2; i < indices.size(); i++) {
    if (i % 2 == 0) {
      fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", indices[i - 2] + 1,
              indices[i - 2] + 1, indices[i - 2] + 1, indices[i - 1] + 1,
              indices[i - 1] + 1, indices[i - 1] + 1, indices[i] + 1,
              indices[i] + 1, indices[i] + 1);
    } else {
      fprintf(f, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", indices[i - 1] + 1,
              indices[i - 1] + 1, indices[i - 1] + 1, indices[i - 2] + 1,
              indices[i - 2] + 1, indices[i - 2] + 1, indices[i] + 1,
              indices[i] + 1, indices[i] + 1);
    }
  }
  fclose(f);

  return 0;
}