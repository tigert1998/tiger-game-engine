#include "obb.h"

#include <glad/glad.h>

#include "utils.h"

bool OBB::IntersectsOBB(const OBB &obb, float epsilon) const {
  // https://github.com/mrdoob/three.js/blob/dev/examples/jsm/math/OBB.js#L150

  glm::vec3 a_c = center_world_space();
  glm::vec3 a_e = extents();
  glm::mat3 a_u = rotation();

  glm::vec3 b_c = obb.center_world_space();
  glm::vec3 b_e = obb.extents();
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
    vec3 a_c = x.rotation * (x.coordsMax + x.coordsMin) * 0.5;
    vec3 a_e = (x.coordsMax - x.coordsMin) * 0.5;
    mat3 a_u = x.rotation;

    vec3 b_c = y.rotation * (y.coordsMax + y.coordsMin) * 0.5;
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

OBBDrawer::OBBDrawer() {
  CompileShaders();

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex),
                        (void *)offsetof(Vertex, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex),
                        (void *)offsetof(Vertex, color));

  for (int i = 0; i < 3; i++) {
    uint32_t location = 2 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(
        location, 3, GL_FLOAT, false, sizeof(Vertex),
        (void *)(offsetof(Vertex, rotation) + i * sizeof(glm::vec3)));
  }
  glBindVertexArray(0);

  CHECK_OPENGL_ERROR();
}

OBBDrawer::~OBBDrawer() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
}

void OBBDrawer::CompileShaders() {
  if (kShader == nullptr) {
    kShader.reset(new Shader(kVsSource, kFsSource, {}));
  }
}

void OBBDrawer::Clear() { vertices_.clear(); }

void OBBDrawer::AppendOBBs(const std::vector<OBB> &obbs,
                           std::optional<glm::vec3> color) {
  const static std::vector<glm::vec3> colors = {
      glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1),
      glm::vec3(1, 1, 0), glm::vec3(1, 0, 1),
  };

  for (int obb_idx = 0; obb_idx < obbs.size(); obb_idx++) {
    const auto &obb = obbs[obb_idx];
    const auto &v_color = color.value_or(colors[obb_idx % colors.size()]);
    for (float a : {obb.min.x, obb.max.x})
      for (float b : {obb.min.y, obb.max.y})
        for (float c : {obb.min.z, obb.max.z})
          for (float x : {obb.min.x, obb.max.x})
            for (float y : {obb.min.y, obb.max.y})
              for (float z : {obb.min.z, obb.max.z})
                if (x >= a && y >= b && z >= c &&
                    (int(x > a) + int(y > b) + int(z > c) == 1)) {
                  Vertex _0;
                  _0.position = glm::vec3(a, b, c);
                  _0.color = v_color;
                  _0.rotation = obb.rotation();

                  Vertex _1;
                  _1.position = glm::vec3(x, y, z);
                  _1.color = v_color;
                  _1.rotation = obb.rotation();

                  vertices_.push_back(_0);
                  vertices_.push_back(_1);
                }
  }
}

void OBBDrawer::AllocateBuffer() {
  if (vertices_.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices_.size(),
                 vertices_.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

void OBBDrawer::Draw(Camera *camera) {
  if (vertices_.size() == 0) return;

  glBindVertexArray(vao_);

  kShader->Use();
  kShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  kShader->SetUniform<glm::mat4>("uProjectionMatrix",
                                 camera->projection_matrix());
  glDrawArrays(GL_LINES, 0, vertices_.size());

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::unique_ptr<Shader> OBBDrawer::kShader = nullptr;

const std::string OBBDrawer::kVsSource = R"(
#version 460 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in mat3 aRotation;

out vec3 vColor;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

void main() {
    vColor = aColor;
    gl_Position = uProjectionMatrix * uViewMatrix * mat4(aRotation) * vec4(aPosition, 1);
}
)";

const std::string OBBDrawer::kFsSource = R"(
#version 460 core

in vec3 vColor;
out vec4 fragColor;

void main() {
    fragColor = vec4(vColor, 1);
}
)";