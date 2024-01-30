#include "obb.h"

bool OBB::IntersectsOBB(const OBB& obb, float epsilon) const {
  // https://github.com/mrdoob/three.js/blob/dev/examples/jsm/math/OBB.js#L150

  glm::vec3 a_c = center();
  float a_e[3] = {extents().x, extents().y, extents().z};
  glm::mat3 a_u = rotation();

  glm::vec3 b_c = obb.center();
  float b_e[3] = {obb.extents().x, obb.extents().y, obb.extents().z};
  glm::mat3 b_u = obb.rotation();

  // compute rotation matrix expressing b in a's coordinate frame

  glm::mat3 R;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      R[i][j] = glm::dot(a_u[i], b_u[j]);
    }
  }

  // compute translation vector

  glm::vec3 v1 = b_c - a_c;

  // bring translation into a's coordinate frame

  glm::vec3 t;
  t[0] = glm::dot(v1, a_u[0]);
  t[1] = glm::dot(v1, a_u[1]);
  t[2] = glm::dot(v1, a_u[2]);

  // compute common subexpressions. Add in an epsilon term to
  // counteract arithmetic errors when two edges are parallel and
  // their cross product is (near) null

  glm::mat3 AbsR;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      AbsR[i][j] = std::abs(R[i][j]) + epsilon;
    }
  }

  float ra, rb;

  // test axes L = A0, L = A1, L = A2

  for (int i = 0; i < 3; i++) {
    ra = a_e[i];
    rb = b_e[0] * AbsR[i][0] + b_e[1] * AbsR[i][1] + b_e[2] * AbsR[i][2];
    if (std::abs(t[i]) > ra + rb) return false;
  }

  // test axes L = B0, L = B1, L = B2

  for (int i = 0; i < 3; i++) {
    ra = a_e[0] * AbsR[0][i] + a_e[1] * AbsR[1][i] + a_e[2] * AbsR[2][i];
    rb = b_e[i];
    if (std::abs(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]) > ra + rb)
      return false;
  }

  // test axis L = A0 x B0

  ra = a_e[1] * AbsR[2][0] + a_e[2] * AbsR[1][0];
  rb = b_e[1] * AbsR[0][2] + b_e[2] * AbsR[0][1];
  if (std::abs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;

  // test axis L = A0 x B1

  ra = a_e[1] * AbsR[2][1] + a_e[2] * AbsR[1][1];
  rb = b_e[0] * AbsR[0][2] + b_e[2] * AbsR[0][0];
  if (std::abs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;

  // test axis L = A0 x B2

  ra = a_e[1] * AbsR[2][2] + a_e[2] * AbsR[1][2];
  rb = b_e[0] * AbsR[0][1] + b_e[1] * AbsR[0][0];
  if (std::abs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;

  // test axis L = A1 x B0

  ra = a_e[0] * AbsR[2][0] + a_e[2] * AbsR[0][0];
  rb = b_e[1] * AbsR[1][2] + b_e[2] * AbsR[1][1];
  if (std::abs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;

  // test axis L = A1 x B1

  ra = a_e[0] * AbsR[2][1] + a_e[2] * AbsR[0][1];
  rb = b_e[0] * AbsR[1][2] + b_e[2] * AbsR[1][0];
  if (std::abs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;

  // test axis L = A1 x B2

  ra = a_e[0] * AbsR[2][2] + a_e[2] * AbsR[0][2];
  rb = b_e[0] * AbsR[1][1] + b_e[1] * AbsR[1][0];
  if (std::abs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;

  // test axis L = A2 x B0

  ra = a_e[0] * AbsR[1][0] + a_e[1] * AbsR[0][0];
  rb = b_e[1] * AbsR[2][2] + b_e[2] * AbsR[2][1];
  if (std::abs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;

  // test axis L = A2 x B1

  ra = a_e[0] * AbsR[1][1] + a_e[1] * AbsR[0][1];
  rb = b_e[0] * AbsR[2][2] + b_e[2] * AbsR[2][0];
  if (std::abs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;

  // test axis L = A2 x B2

  ra = a_e[0] * AbsR[1][2] + a_e[1] * AbsR[0][2];
  rb = b_e[0] * AbsR[2][1] + b_e[1] * AbsR[2][0];
  if (std::abs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;

  // since no separating axis is found, the OBBs must be intersecting

  return true;
}

const std::string OBB::GLSLSource() {
  return R"(
struct OBB {
    vec3 coordsMin;
    vec3 coordsMax;
    mat3 rotation;
};

bool IntersectsOBB(OBB x, OBB y, float epsilon) {
    vec3 a_c = (x.coordsMax + x.coordsMin) * 0.5;
    vec3 a_e = (x.coordsMax - x.coordsMin) * 0.5;
    mat3 a_u = x.rotation;

    vec3 b_c = (y.coordsMax + y.coordsMin) * 0.5;
    vec3 b_e = (y.coordsMax - y.coordsMin) * 0.5;
    mat3 b_u = y.rotation;

    // compute rotation matrix expressing b in a's coordinate frame

    mat3 R;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        R[i][j] = dot(a_u[i], b_u[j]);
      }
    }

    // compute translation vector

    vec3 v1 = b_c - a_c;

    // bring translation into a's coordinate frame

    vec3 t;
    t[0] = dot(v1, a_u[0]);
    t[1] = dot(v1, a_u[1]);
    t[2] = dot(v1, a_u[2]);

    // compute common subexpressions. Add in an epsilon term to
    // counteract arithmetic errors when two edges are parallel and
    // their cross product is (near) null

    mat3 AbsR;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        AbsR[i][j] = abs(R[i][j]) + epsilon;
      }
    }

    float ra, rb;

    // test axes L = A0, L = A1, L = A2

    for (int i = 0; i < 3; i++) {
      ra = a_e[i];
      rb = b_e[0] * AbsR[i][0] + b_e[1] * AbsR[i][1] + b_e[2] * AbsR[i][2];
      if (abs(t[i]) > ra + rb) return false;
    }

    // test axes L = B0, L = B1, L = B2

    for (int i = 0; i < 3; i++) {
      ra = a_e[0] * AbsR[0][i] + a_e[1] * AbsR[1][i] + a_e[2] * AbsR[2][i];
      rb = b_e[i];
      if (abs(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]) > ra + rb) return false;
    }

    // test axis L = A0 x B0

    ra = a_e[1] * AbsR[2][0] + a_e[2] * AbsR[1][0];
    rb = b_e[1] * AbsR[0][2] + b_e[2] * AbsR[0][1];
    if (abs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;

    // test axis L = A0 x B1

    ra = a_e[1] * AbsR[2][1] + a_e[2] * AbsR[1][1];
    rb = b_e[0] * AbsR[0][2] + b_e[2] * AbsR[0][0];
    if (abs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;

    // test axis L = A0 x B2

    ra = a_e[1] * AbsR[2][2] + a_e[2] * AbsR[1][2];
    rb = b_e[0] * AbsR[0][1] + b_e[1] * AbsR[0][0];
    if (abs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;

    // test axis L = A1 x B0

    ra = a_e[0] * AbsR[2][0] + a_e[2] * AbsR[0][0];
    rb = b_e[1] * AbsR[1][2] + b_e[2] * AbsR[1][1];
    if (abs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;

    // test axis L = A1 x B1

    ra = a_e[0] * AbsR[2][1] + a_e[2] * AbsR[0][1];
    rb = b_e[0] * AbsR[1][2] + b_e[2] * AbsR[1][0];
    if (abs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;

    // test axis L = A1 x B2

    ra = a_e[0] * AbsR[2][2] + a_e[2] * AbsR[0][2];
    rb = b_e[0] * AbsR[1][1] + b_e[1] * AbsR[1][0];
    if (abs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;

    // test axis L = A2 x B0

    ra = a_e[0] * AbsR[1][0] + a_e[1] * AbsR[0][0];
    rb = b_e[1] * AbsR[2][2] + b_e[2] * AbsR[2][1];
    if (abs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;

    // test axis L = A2 x B1

    ra = a_e[0] * AbsR[1][1] + a_e[1] * AbsR[0][1];
    rb = b_e[0] * AbsR[2][2] + b_e[2] * AbsR[2][0];
    if (abs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;

    // test axis L = A2 x B2

    ra = a_e[0] * AbsR[1][2] + a_e[1] * AbsR[0][2];
    rb = b_e[0] * AbsR[2][1] + b_e[1] * AbsR[2][0];
    if (abs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;

    // since no separating axis is found, the OBBs must be intersecting

    return true;
}
)";
}