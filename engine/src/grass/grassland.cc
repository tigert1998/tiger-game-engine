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

uniform uint uNumTriangles;

const float PI = radians(180);

void main() {
    if (gl_GlobalInvocationID.x >= uNumTriangles) {
        return;
    }
    uint a = indices[gl_GlobalInvocationID.x * 3 + 0];
    uint b = indices[gl_GlobalInvocationID.x * 3 + 1];
    uint c = indices[gl_GlobalInvocationID.x * 3 + 2];

    for (int i = 0; i < 8; i++) {
        float width = mix(0.15, 0.3, rands[gl_GlobalInvocationID.x * 56 + i * 7 + 0]);
        float height = mix(0.5, 1.2, rands[gl_GlobalInvocationID.x * 56 + i * 7 + 1]);
        float bend = mix(-PI / 6, PI / 6, rands[gl_GlobalInvocationID.x * 56 + i * 7 + 2]);
        float theta = mix(0, 2 * PI, rands[gl_GlobalInvocationID.x * 56 + i * 7 + 3]);

        vec3 coord = vec3(
            rands[gl_GlobalInvocationID.x * 56 + i * 7 + 4],
            rands[gl_GlobalInvocationID.x * 56 + i * 7 + 5],
            rands[gl_GlobalInvocationID.x * 56 + i * 7 + 6]
        );
        vec3 trianglePosition = mat3(vertices[a], vertices[b], vertices[c]) *
            (coord / (coord.x + coord.y + coord.z));

        bladeTransforms[gl_GlobalInvocationID.x * 8 + i] =
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
}
)";

constexpr int32_t kBladesPerTriangle = 8;

Grassland::Grassland(const std::string& terrain_model_path)
    : blade_(std::make_unique<Blade>()) {
  // load mesh and prepare data

  LOG(INFO) << "loading terrain at: \"" << terrain_model_path << "\"";
  const aiScene* scene =
      aiImportFile(terrain_model_path.c_str(),
                   aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
                       aiProcess_Triangulate | aiProcess_GenNormals);
  CHECK_EQ(scene->mNumMeshes, 1);
  auto mesh = scene->mMeshes[0];

  std::vector<glm::vec4> vertices;
  vertices.reserve(mesh->mNumVertices);
  vertices_for_bvh_.reserve(mesh->mNumVertices);
  for (int i = 0; i < mesh->mNumVertices; i++) {
    vertices.emplace_back(mesh->mVertices[i].x, mesh->mVertices[i].y,
                          mesh->mVertices[i].z, 1);
    vertices_for_bvh_.push_back({vertices.back()});
  }
  std::vector<uint32_t> indices;
  indices.reserve(mesh->mNumFaces * 3);
  triangles_for_bvh_.reserve(mesh->mNumFaces);
  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
    triangles_for_bvh_.emplace_back(face.mIndices[0], face.mIndices[1],
                                    face.mIndices[2]);
  }
  uint32_t num_triangles = triangles_for_bvh_.size();
  std::vector<float> rands(num_triangles * kBladesPerTriangle * 7);
  std::mt19937 rd;
  for (int i = 0; i < rands.size(); i++) {
    rands[i] = std::uniform_real_distribution<float>(0, 1)(rd);
  }

  // launch compute shader to calculate blade transforms once and for all

  auto calc_blade_transforms_shader = std::unique_ptr<Shader>(
      new Shader({{GL_COMPUTE_SHADER, Grassland::kCsSource}}));
  uint32_t vertices_ssbo, indices_ssbo, blade_transforms_ssbo, rands_ssbo;

  glGenBuffers(1, &vertices_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertices_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec4),
               glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
  // OpenGL sets vec3 alignment to 4N for SSBO, so we must pass glm::vec4
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vertices_ssbo);
  // vertices must have binding = 0

  glGenBuffers(1, &indices_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, indices_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(uint32_t),
               &indices[0], GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indices_ssbo);
  // indices must have binding = 1

  glGenBuffers(1, &blade_transforms_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               num_triangles * kBladesPerTriangle * sizeof(glm::mat4), nullptr,
               GL_STATIC_READ);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, blade_transforms_ssbo);
  // blade_transforms must have binding = 2

  glGenBuffers(1, &rands_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, rands_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, rands.size() * sizeof(float),
               rands.data(), GL_STATIC_READ);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, rands_ssbo);
  // blade_transforms must have binding = 3
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  calc_blade_transforms_shader->Use();
  calc_blade_transforms_shader->SetUniform<uint32_t>("uNumTriangles",
                                                     num_triangles);
  glDispatchCompute((num_triangles + 63) / 64, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_ATOMIC_COUNTER_BARRIER_BIT);

  // copy data from GPU memory to CPU memory

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo);
  glm::mat4* buffer =
      (glm::mat4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
  blade_transforms_.resize(kBladesPerTriangle * num_triangles);
  std::copy(buffer, buffer + blade_transforms_.size(),
            blade_transforms_.data());
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  // delete buffers

  glDeleteBuffers(1, &vertices_ssbo);
  glDeleteBuffers(1, &indices_ssbo);
  glDeleteBuffers(1, &blade_transforms_ssbo);
  glDeleteBuffers(1, &rands_ssbo);

  bvh_.reset(new BVH<VertexType>(vertices_for_bvh_.data(),
                                 triangles_for_bvh_.data(), num_triangles));

  glGenBuffers(1, &vbo_);
}

Grassland::~Grassland() { glDeleteBuffers(1, &vbo_); }

void Grassland::Draw(Camera* camera, LightSources* light_sources) {
  // copy blade_transforms_ssbo to vbo
  std::vector<glm::mat4> buffer;
  bvh_->Search(camera->frustum(), [&](BVHNode* node) {
    for (int i = 0; i < node->num_triangles; i++) {
      uint32_t triangle_index = node->triangle_indices[i];
      buffer.push_back(
          this->blade_transforms_[triangle_index * kBladesPerTriangle]);
    }
  });

  glBindVertexArray(blade_->vao());
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(glm::mat4),
               glm::value_ptr(buffer[0]), GL_DYNAMIC_DRAW);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

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
                          0, buffer.size());
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}