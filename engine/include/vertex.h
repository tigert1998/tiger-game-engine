#ifndef VERTEX_H_
#define VERTEX_H_

#include <stdint.h>

#include <glm/glm.hpp>

#include "cg_exception.h"

#define IMPLEMENT_METHODS                                         \
  int bone_ids[MaxBonesPerVertex];                                \
  float bone_weights[MaxBonesPerVertex];                          \
  Vertex() {                                                      \
    std::fill(bone_ids, bone_ids + MaxBonesPerVertex, -1);        \
    std::fill(bone_weights, bone_weights + MaxBonesPerVertex, 0); \
  }                                                               \
  int NumBones() {                                                \
    for (int i = 0; i < MaxBonesPerVertex; i++)                   \
      if (bone_ids[i] < 0) return i;                              \
    return MaxBonesPerVertex;                                     \
  }                                                               \
  void AddBone(int id, float weight) {                            \
    int i = NumBones();                                           \
    if (i >= MaxBonesPerVertex) throw MaxBoneExceededError();     \
    bone_weights[i] = weight;                                     \
    bone_ids[i] = id;                                             \
  }

template <int MaxBonesPerVertex, bool HasTexCoord = true>
class Vertex {
 public:
  glm::vec3 position;
  glm::vec2 tex_coord;
  glm::vec3 normal;

  IMPLEMENT_METHODS
};

template <int MaxBonesPerVertex>
class Vertex<MaxBonesPerVertex, false> {
 public:
  glm::vec3 position;
  glm::vec3 normal;

  IMPLEMENT_METHODS
};

template <>
class Vertex<0, true> {
 public:
  glm::vec3 position;
  glm::vec2 tex_coord;
  glm::vec3 normal;
};

template <>
class Vertex<0, false> {
 public:
  glm::vec3 position;
  glm::vec3 normal;
};

#undef IMPLEMENT_METHODS

#endif