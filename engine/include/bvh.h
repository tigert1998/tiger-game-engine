#ifndef BVH_H_
#define BVH_H_

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "aabb.h"

struct BVHNode {
  AABB aabb;
  uint32_t split_axis = -1;
  std::unique_ptr<BVHNode> left = nullptr, right = nullptr;
  uint32_t* triangle_indices;
  uint32_t num_triangles;
};

template <typename V>
class BVH {
 private:
  static constexpr uint32_t kMinTrianglesInNode = 1 << 12;

  std::unique_ptr<BVHNode> root_ = nullptr;
  std::vector<uint32_t> triangle_indices_;

  static AABB GetBV(const V* vertices, const glm::uvec3* triangles,
                    const uint32_t* triangle_indices, uint32_t num_triangles) {
    AABB aabb;
    aabb.min = glm::vec3(INFINITY);
    aabb.max = glm::vec3(-INFINITY);
    for (int i = 0; i < num_triangles; i++)
      for (int j = 0; j < 3; j++)
        for (int k = 0; k < 3; k++) {
          aabb.min[k] =
              std::min(vertices[triangles[triangle_indices[i]][j]].position[k],
                       aabb.min[k]);
          aabb.max[k] =
              std::max(vertices[triangles[triangle_indices[i]][j]].position[k],
                       aabb.max[k]);
        }
    return aabb;
  }

  static std::unique_ptr<BVHNode> BuildRecursively(const V* vertices,
                                                   const glm::uvec3* triangles,
                                                   uint32_t* triangle_indices,
                                                   uint32_t num_triangles) {
    auto node = std::unique_ptr<BVHNode>(new BVHNode());
    node->aabb =
        BVH<V>::GetBV(vertices, triangles, triangle_indices, num_triangles);
    node->triangle_indices = triangle_indices;
    node->num_triangles = num_triangles;
    if (num_triangles <= kMinTrianglesInNode) {
      return node;
    }

    // calculate split axis
    glm::vec3 range = node->aabb.max - node->aabb.min;
    node->split_axis = 0;
    for (int i = 1; i <= 2; i++) {
      if (range[i] > range[node->split_axis]) {
        node->split_axis = i;
      }
    }

    std::sort(triangle_indices, triangle_indices + num_triangles,
              [&](uint32_t a, uint32_t b) {
                glm::vec3 a_center = (vertices[triangles[a][0]].position +
                                      vertices[triangles[a][1]].position +
                                      vertices[triangles[a][2]].position) /
                                     3.0f;
                glm::vec3 b_center = (vertices[triangles[b][0]].position +
                                      vertices[triangles[b][1]].position +
                                      vertices[triangles[b][2]].position) /
                                     3.0f;
                return a_center[node->split_axis] < b_center[node->split_axis];
              });

    node->left = BuildRecursively(vertices, triangles, triangle_indices,
                                  num_triangles / 2);
    node->right = BuildRecursively(vertices, triangles,
                                   triangle_indices + num_triangles / 2,
                                   (num_triangles + 1) / 2);
    return node;
  }

  static void TraverseRecursively(
      BVHNode* node, const std::function<void(BVHNode*)>& callback) {
    if (node->split_axis == -1) {
      callback(node);
    } else {
      TraverseRecursively(node->left.get(), callback);
      TraverseRecursively(node->right.get(), callback);
    }
  }

  static void SearchRecursively(BVHNode* node, const Frustum& frustum,
                                const std::function<void(BVHNode*)>& callback) {
    if (!node->aabb.IsOnFrustum(frustum)) return;
    if (node->split_axis == -1) {
      callback(node);
    } else {
      SearchRecursively(node->left.get(), frustum, callback);
      SearchRecursively(node->right.get(), frustum, callback);
    }
  }

 public:
  BVH(const V* vertices, const glm::uvec3* triangles, uint32_t num_triangles) {
    triangle_indices_.resize(num_triangles);
    for (int i = 0; i < num_triangles; i++) triangle_indices_[i] = i;

    root_ = BuildRecursively(vertices, triangles, triangle_indices_.data(),
                             num_triangles);
  }

  void Traverse(const std::function<void(BVHNode*)>& callback) {
    TraverseRecursively(root_.get(), callback);
  }

  void Search(const Frustum& frustum,
              const std::function<void(BVHNode*)>& callback) {
    SearchRecursively(root_.get(), frustum, callback);
  }
};

#endif