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

std::string AABB::GLSLSource() {
  return Frustum::GLSLSource() + R"(
const float FLT_MAX = 3.402823466e+38;
const float FLT_MIN = 1.175494351e-38;

struct AABB {
    vec3 coordsMin;
    vec3 coordsMax;
};

AABB TransformAABB(mat4 transform, AABB aabb) {
    AABB ret;
    ret.coordsMin = vec3(FLT_MAX);
    ret.coordsMax = vec3(-FLT_MAX);
    float axis0[] = {aabb.coordsMin[0], aabb.coordsMax[0]};
    float axis1[] = {aabb.coordsMin[1], aabb.coordsMax[1]};
    float axis2[] = {aabb.coordsMin[2], aabb.coordsMax[2]};
    for (int i = 0; i < 2; i++) {
        float a = axis0[i];
        for (int j = 0; j < 2; j++) {
            float b = axis1[j];
            for (int k = 0; k < 2; k++) {
                float c = axis2[k];
                vec3 tmp = vec3(transform * vec4(a, b, c, 1));
                ret.min = min(ret.min, tmp);
                ret.max = max(ret.max, tmp);
            }
        }
    }
    return ret;
}

bool AABBIsOnOrForwardPlane(AABB aabb, FrustumPlane plane) {
    vec3 planeNormal = vec3(plane.normal[0], plane.normal[1], plane.normal[2]);
    vec3 extent = (aabb.coordsMax - aabb.coordsMin) * 0.5;
    float r = dot(extent, planeNormal);
    vec3 center = (aabb.coordsMax + aabb.coordsMin) * 0.5;
    return dot(planeNormal, center) - plane.distance >= -r;
}

bool AABBIsOnFrustum(AABB aabb, Frustum frustum) {
    return AABBIsOnOrForwardPlane(frustum.topPlane) &&
        AABBIsOnOrForwardPlane(frustum.bottomPlane) &&
        AABBIsOnOrForwardPlane(frustum.nearPlane) &&
        AABBIsOnOrForwardPlane(frustum.farPlane) &&
        AABBIsOnOrForwardPlane(frustum.leftPlane) &&
        AABBIsOnOrForwardPlane(frustum.rightPlane);
}
)";
}