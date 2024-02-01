#ifndef AABB_H_
#define AABB_H_

#include <glm/glm.hpp>

#include "camera.h"

struct AABB {
  glm::vec3 min;
  alignas(16) glm::vec3 max;

  inline glm::vec3 center() const { return (max + min) * 0.5f; }
  inline glm::vec3 extents() const { return max - center(); }

  bool IsOnOrForwardPlane(const FrustumPlane& plane) const;
  bool IsOnFrustum(const Frustum& frustum) const;
  AABB Transform(const glm::mat4& transform) const;

  static std::string GLSLSource();
};

#endif