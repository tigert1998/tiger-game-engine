#include "grass/grassland.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glad/glad.h>
#include <glog/logging.h>

#include <assimp/Importer.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "utils.h"
#include "vertex.h"

const std::string Grassland::kCsSource = R"(
#version 430 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0) buffer verticesBuffer {
    vec3 vertices[];
};
layout (binding = 1) buffer indicesBuffer {
    uint indices[];
};
layout (binding = 2) buffer bladeTransformsBuffer {
    mat4 bladeTransforms[];
};

layout (binding = 0) uniform atomic_uint uNumBlades;

uniform uint uNumTriangles;

void main() {
    if (gl_GlobalInvocationID.x >= uNumTriangles) {
        return;
    }
    uint a = indices[gl_GlobalInvocationID.x * 3 + 0];
    uint b = indices[gl_GlobalInvocationID.x * 3 + 1];
    uint c = indices[gl_GlobalInvocationID.x * 3 + 2];

    uint index = atomicCounterIncrement(uNumBlades);

    vec3 triangleCenter = (vertices[a] + vertices[b] + vertices[c]) / 3.0;
    bladeTransforms[index] = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(triangleCenter, 1.0)
    );
}
)";

constexpr float kBladesPerTriangle = 3;

Grassland::Grassland(const std::string& terrain_model_path) {
  calc_blade_transforms_shader_.reset(
      new Shader({{GL_COMPUTE_SHADER, Grassland::kCsSource}}));

  LOG(INFO) << "loading terrain at: \"" << terrain_model_path << "\"";
  const aiScene* scene =
      aiImportFile(terrain_model_path.c_str(),
                   aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
                       aiProcess_Triangulate | aiProcess_GenNormals);
  CHECK_EQ(scene->mNumMeshes, 1);
  auto mesh = scene->mMeshes[0];

  std::vector<glm::vec4> vertices;
  vertices.reserve(mesh->mNumVertices);
  for (int i = 0; i < mesh->mNumVertices; i++) {
    vertices.emplace_back(mesh->mVertices[i].x, mesh->mVertices[i].y,
                          mesh->mVertices[i].z, 1);
  }
  std::vector<uint32_t> indices;
  indices.reserve(mesh->mNumFaces * 3);
  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }
  num_triangles_ = indices.size() / 3;

  glGenBuffers(1, &vertices_ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertices_ssbo_);
  glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec4),
               glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
  // SSBO set vec3 alignment to 4N, so we must pass glm::vec4

  glGenBuffers(1, &indices_ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, indices_ssbo_);
  glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(uint32_t),
               &indices[0], GL_STATIC_DRAW);

  glGenBuffers(1, &blade_transforms_ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo_);
  glBufferData(
      GL_SHADER_STORAGE_BUFFER,
      (int32_t)(num_triangles_ * kBladesPerTriangle) * sizeof(glm::mat4),
      nullptr, GL_STATIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  glGenBuffers(1, &num_blades_buffer_);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, num_blades_buffer_);
  glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}

Grassland::~Grassland() {
  glDeleteBuffers(1, &vertices_ssbo_);
  glDeleteBuffers(1, &indices_ssbo_);
  glDeleteBuffers(1, &blade_transforms_ssbo_);
  glDeleteBuffers(1, &num_blades_buffer_);
}

void Grassland::CalcBladeTransforms() {
  calc_blade_transforms_shader_->Use();
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertices_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vertices_ssbo_);
  // vertices must have binding = 0

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, indices_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indices_ssbo_);
  // indices must have binding = 1

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, blade_transforms_ssbo_);
  // blade_transforms must have binding = 2

  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, num_blades_buffer_);
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, num_blades_buffer_);
  // num_blades must have binding = 0
  const uint32_t zero = 0;
  glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(zero), &zero);

  calc_blade_transforms_shader_->SetUniform<uint32_t>("uNumTriangles",
                                                      num_triangles_);

  glDispatchCompute((num_triangles_ + 63) / 64, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_ATOMIC_COUNTER_BARRIER_BIT);
}