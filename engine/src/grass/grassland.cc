#include "grass/grassland.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glad/glad.h>
#include <glog/logging.h>

#include <assimp/Importer.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>

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
layout (binding = 3) buffer randsBuffer {
    float rands[];
};

layout (binding = 0) uniform atomic_uint uNumBlades;

uniform uint uNumTriangles;

const float PI = radians(180);

void main() {
    if (gl_GlobalInvocationID.x >= uNumTriangles) {
        return;
    }
    uint a = indices[gl_GlobalInvocationID.x * 3 + 0];
    uint b = indices[gl_GlobalInvocationID.x * 3 + 1];
    uint c = indices[gl_GlobalInvocationID.x * 3 + 2];

    uint index = atomicCounterIncrement(uNumBlades);

    float width = mix(0.15, 0.3, rands[gl_GlobalInvocationID.x * 56 + 0]);
    float height = mix(0.5, 1.2, rands[gl_GlobalInvocationID.x * 56 + 1]);
    float bend = mix(-PI / 6, PI / 6, rands[gl_GlobalInvocationID.x * 56 + 2]);
    float theta = mix(0, 2 * PI, rands[gl_GlobalInvocationID.x * 56 + 3]);

    vec3 coord = vec3(
        rands[gl_GlobalInvocationID.x * 56 + 4],
        rands[gl_GlobalInvocationID.x * 56 + 5],
        rands[gl_GlobalInvocationID.x * 56 + 6]
    );
    vec3 trianglePosition = mat3(vertices[a], vertices[b], vertices[c]) *
        (coord / (coord.x + coord.y + coord.z));

    bladeTransforms[gl_GlobalInvocationID.x] =
    mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(trianglePosition, 1.0)
    ) * mat4(
        vec4(cos(theta), 0.0, -sin(theta), 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(sin(theta), 0.0, cos(theta), 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    ) * mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, cos(bend), sin(bend), 0.0),
        vec4(0.0, -sin(bend), cos(bend), 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    ) * mat4(
        vec4(width, 0.0, 0.0, 0.0),
        vec4(0.0, height, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
}
)";

constexpr float kBladesPerTriangle = 3;

Grassland::Grassland(const std::string& terrain_model_path) {
  calc_blade_transforms_shader_.reset(
      new Shader({{GL_COMPUTE_SHADER, Grassland::kCsSource}}));
  blade_.reset(new Blade());

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
  std::vector<float> rands(num_triangles_ * 56);
  std::mt19937 rd;
  for (int i = 0; i < rands.size(); i++) {
    rands[i] = std::uniform_real_distribution<float>(0, 1)(rd);
  }

  glGenBuffers(1, &vertices_ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertices_ssbo_);
  glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec4),
               glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
  // OpenGL sets vec3 alignment to 4N for SSBO, so we must pass glm::vec4

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

  glGenBuffers(1, &rands_ssbo_);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, rands_ssbo_);
  glBufferData(GL_SHADER_STORAGE_BUFFER, rands.size() * sizeof(float),
               rands.data(), GL_STATIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  glGenBuffers(1, &num_blades_buffer_);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, num_blades_buffer_);
  glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr,
               GL_DYNAMIC_READ);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

  glGenBuffers(1, &vbo_);
}

Grassland::~Grassland() {
  glDeleteBuffers(1, &vertices_ssbo_);
  glDeleteBuffers(1, &indices_ssbo_);
  glDeleteBuffers(1, &blade_transforms_ssbo_);
  glDeleteBuffers(1, &num_blades_buffer_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &rands_ssbo_);
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

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, rands_ssbo_);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, rands_ssbo_);
  // blade_transforms must have binding = 3

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

void Grassland::Draw(Camera* camera, LightSources* light_sources) {
  CalcBladeTransforms();

  // copy blade_transforms_ssbo_ to vbo
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, num_blades_buffer_);
  uint32_t num_blades = 0;
  glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(num_blades),
                     &num_blades);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo_);
  glm::mat4* buffer =
      (glm::mat4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
  glBindVertexArray(blade_->vao());
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, num_blades * sizeof(glm::mat4), buffer,
               GL_DYNAMIC_DRAW);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // draw blades with calculated transforms
  blade_->shader()->Use();
  light_sources->Set(blade_->shader());
  // bind vbo to glsl
  glBindVertexArray(blade_->vao());
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  for (int i = 0; i < 4; i++) {
    uint32_t location = 3 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)(i * sizeof(glm::vec4)));
    glVertexAttribDivisor(location, 1);
  }
  // set uniforms
  blade_->shader()->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  blade_->shader()->SetUniform<glm::mat4>("uProjectionMatrix",
                                          camera->projection_matrix());
  blade_->shader()->SetUniform<glm::vec3>("uCameraPosition",
                                          camera->position());
  // draw blades
  glDrawElementsInstanced(GL_TRIANGLES, blade_->indices_size(), GL_UNSIGNED_INT,
                          0, num_blades);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}