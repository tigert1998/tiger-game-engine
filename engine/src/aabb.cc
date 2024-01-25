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