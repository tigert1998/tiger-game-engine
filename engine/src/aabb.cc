#include "aabb.h"

#include <iostream>

bool AABB::IsOnOrForwardPlane(const FrustumPlane& plane) const {
  const float r = glm::dot(extents(), glm::abs(plane.normal));
  return plane.GetSignedDistanceToPlane(center()) >= -r;
}

bool AABB::IsOnFrustum(const Frustum& frustum) const {
  return IsOnOrForwardPlane(frustum.top_plane) &&
         IsOnOrForwardPlane(frustum.bottom_plane) &&
         IsOnOrForwardPlane(frustum.near_plane) &&
         IsOnOrForwardPlane(frustum.far_plane) &&
         IsOnOrForwardPlane(frustum.left_plane) &&
         IsOnOrForwardPlane(frustum.right_plane);
}

AABB AABB::Transform(const glm::mat4& transform) const {
  AABB ret;
  ret.min = glm::vec3((std::numeric_limits<float>::max)());
  ret.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (float a : {min[0], max[0]})
    for (float b : {min[1], max[1]})
      for (float c : {min[2], max[2]}) {
        glm::vec3 tmp = glm::vec3(transform * glm::vec4(a, b, c, 1));
        ret.min = (glm::min)(ret.min, tmp);
        ret.max = (glm::max)(ret.max, tmp);
      }
  return ret;
}
