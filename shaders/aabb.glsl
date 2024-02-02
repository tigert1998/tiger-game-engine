#include "frustum.glsl"

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
                ret.coordsMin = min(ret.coordsMin, tmp);
                ret.coordsMax = max(ret.coordsMax, tmp);
            }
        }
    }
    return ret;
}

bool AABBIsOnOrForwardPlane(AABB aabb, FrustumPlane plane) {
    vec3 planeNormal = vec3(plane.normal[0], plane.normal[1], plane.normal[2]);
    vec3 extent = (aabb.coordsMax - aabb.coordsMin) * 0.5;
    float r = dot(extent, abs(planeNormal));
    vec3 center = (aabb.coordsMax + aabb.coordsMin) * 0.5;
    return dot(planeNormal, center) - plane.distance >= -r;
}

bool AABBIsOnFrustum(AABB aabb, Frustum frustum) {
    return AABBIsOnOrForwardPlane(aabb, frustum.topPlane) &&
        AABBIsOnOrForwardPlane(aabb, frustum.bottomPlane) &&
        AABBIsOnOrForwardPlane(aabb, frustum.nearPlane) &&
        AABBIsOnOrForwardPlane(aabb, frustum.farPlane) &&
        AABBIsOnOrForwardPlane(aabb, frustum.leftPlane) &&
        AABBIsOnOrForwardPlane(aabb, frustum.rightPlane);
}
