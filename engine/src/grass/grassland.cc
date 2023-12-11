#define NOMINMAX

#include "grass/grassland.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glog/logging.h>

#include <algorithm>
#include <assimp/Importer.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>

#include "texture_manager.h"
#include "utils.h"
#include "vertex.h"

const std::string Grassland::kCsSource = R"(
#version 430 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// inputs
layout (binding = 0) buffer verticesBuffer {
    vec3 vertices[];
};
layout (binding = 1) buffer indicesBuffer {
    uint indices[];
};
layout (binding = 2) buffer texCoordsBuffer {
    vec2 texCoords[];
};
layout (binding = 3) buffer randsBuffer {
    float rands[];
};

// outputs
layout (binding = 4) buffer bladeTransformsBuffer {
    mat4 bladeTransforms[];
};
layout (binding = 5) buffer bladePositionsBuffer {
    vec3 bladePositions[];
};
layout (binding = 6) buffer bladeTexCoordsBuffer {
    vec2 bladeTexCoords[];
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
        coord = coord / (coord.x + coord.y + coord.z);

        bladePositions[gl_GlobalInvocationID.x * 8 + i] = 
        mat3(vertices[a], vertices[b], vertices[c]) * coord;

        bladeTexCoords[gl_GlobalInvocationID.x * 8 + i] =
        mat3x2(texCoords[a], texCoords[b], texCoords[c]) * coord;

        bladeTransforms[gl_GlobalInvocationID.x * 8 + i] =
        mat4(
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

constexpr int32_t kMaxLOD = 8;

uint32_t Grassland::CreateAndBindSSBO(uint32_t size, void* data, uint32_t usage,
                                      uint32_t index) {
  uint32_t ssbo;
  glGenBuffers(1, &ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return ssbo;
}

Grassland::Grassland(const std::string& terrain_model_path,
                     const std::string& distortion_texture_path)
    : blade_(std::make_unique<Blade>()) {
  distortion_texture_ = Texture(distortion_texture_path, GL_REPEAT,
                                GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, {}, true);

  // load mesh and prepare data

  LOG(INFO) << "loading terrain at: \"" << terrain_model_path << "\"";
  const aiScene* scene =
      aiImportFile(terrain_model_path.c_str(),
                   aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
                       aiProcess_Triangulate | aiProcess_GenNormals);
  CHECK_EQ(scene->mNumMeshes, 1);
  auto mesh = scene->mMeshes[0];

  std::vector<glm::vec4> vertices;
  std::vector<glm::vec2> tex_coords;
  vertices.reserve(mesh->mNumVertices);
  tex_coords.reserve(mesh->mNumVertices);
  vertices_for_bvh_.reserve(mesh->mNumVertices);
  for (int i = 0; i < mesh->mNumVertices; i++) {
    vertices.emplace_back(mesh->mVertices[i].x, mesh->mVertices[i].y,
                          mesh->mVertices[i].z, 1);
    tex_coords.emplace_back(mesh->mTextureCoords[0][i].x,
                            mesh->mTextureCoords[0][i].y);
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
  std::vector<float> rands(num_triangles * kMaxLOD * 7);
  std::mt19937 rd;
  for (int i = 0; i < rands.size(); i++) {
    rands[i] = std::uniform_real_distribution<float>(0, 1)(rd);
  }

  // launch compute shader to calculate blade transforms once and for all

  auto calc_blade_transforms_shader = std::unique_ptr<Shader>(
      new Shader({{GL_COMPUTE_SHADER, Grassland::kCsSource}}, {}));
  uint32_t vertices_ssbo = Grassland::CreateAndBindSSBO(
      vertices.size() * sizeof(glm::vec4), glm::value_ptr(vertices[0]),
      GL_STATIC_DRAW, 0);
  uint32_t indices_ssbo = Grassland::CreateAndBindSSBO(
      indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW, 1);
  uint32_t tex_coords_ssbo = Grassland::CreateAndBindSSBO(
      tex_coords.size() * sizeof(glm::vec2), glm::value_ptr(tex_coords[0]),
      GL_STATIC_DRAW, 2);
  uint32_t rands_ssbo = Grassland::CreateAndBindSSBO(
      rands.size() * sizeof(float), rands.data(), GL_STATIC_READ, 3);

  uint32_t blade_transforms_ssbo = Grassland::CreateAndBindSSBO(
      num_triangles * kMaxLOD * sizeof(glm::mat4), nullptr, GL_STATIC_READ, 4);
  uint32_t blade_positions_ssbo = Grassland::CreateAndBindSSBO(
      num_triangles * kMaxLOD * sizeof(glm::vec4), nullptr, GL_STATIC_READ, 5);
  uint32_t blade_tex_coords_ssbo = Grassland::CreateAndBindSSBO(
      num_triangles * kMaxLOD * sizeof(glm::vec2), nullptr, GL_STATIC_READ, 6);

  calc_blade_transforms_shader->Use();
  calc_blade_transforms_shader->SetUniform<uint32_t>("uNumTriangles",
                                                     num_triangles);
  glDispatchCompute((num_triangles + 63) / 64, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_ATOMIC_COUNTER_BARRIER_BIT);

  // copy data from GPU memory to CPU memory

  bvh_.reset(new BVH<VertexType>(vertices_for_bvh_.data(),
                                 triangles_for_bvh_.data(), num_triangles));
  {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_transforms_ssbo);
    glm::mat4* buffer =
        (glm::mat4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    bvh_->Traverse([&](BVHNode* node) {
      blade_data_[node].resize(kMaxLOD * node->num_triangles);
      for (int lod = 0; lod < kMaxLOD; lod++) {
        for (int i = 0; i < node->num_triangles; i++) {
          blade_data_[node][lod * node->num_triangles + i].transform =
              buffer[node->triangle_indices[i] * kMaxLOD + lod];
        }
      }
    });
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  }
  {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_positions_ssbo);
    glm::vec4* buffer =
        (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    bvh_->Traverse([&](BVHNode* node) {
      for (int lod = 0; lod < kMaxLOD; lod++) {
        for (int i = 0; i < node->num_triangles; i++) {
          blade_data_[node][lod * node->num_triangles + i].position =
              buffer[node->triangle_indices[i] * kMaxLOD + lod];
        }
      }
    });
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  }
  {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, blade_tex_coords_ssbo);
    glm::vec2* buffer =
        (glm::vec2*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    bvh_->Traverse([&](BVHNode* node) {
      for (int lod = 0; lod < kMaxLOD; lod++) {
        for (int i = 0; i < node->num_triangles; i++) {
          blade_data_[node][lod * node->num_triangles + i].tex_coord =
              buffer[node->triangle_indices[i] * kMaxLOD + lod];
        }
      }
      std::shuffle(blade_data_[node].begin(), blade_data_[node].end(), rd);
    });
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  }

  // delete buffers

  glDeleteBuffers(1, &vertices_ssbo);
  glDeleteBuffers(1, &indices_ssbo);
  glDeleteBuffers(1, &tex_coords_ssbo);
  glDeleteBuffers(1, &rands_ssbo);
  glDeleteBuffers(1, &blade_transforms_ssbo);
  glDeleteBuffers(1, &blade_positions_ssbo);
  glDeleteBuffers(1, &blade_tex_coords_ssbo);

  // release scene

  aiReleaseImport(scene);

  // create vbo

  blade_data_for_gpu_.resize(num_triangles * kMaxLOD);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               blade_data_for_gpu_.size() * sizeof(blade_data_for_gpu_[0]),
               nullptr, GL_STREAM_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Grassland::~Grassland() { glDeleteBuffers(1, &vbo_); }

void Grassland::Draw(Camera* camera, LightSources* light_sources, double time) {
  // copy blade_transforms to vbo
  uint32_t num_blades = 0;
  bvh_->Search(camera->frustum(), [&](BVHNode* node) {
    float distance = glm::distance(node->aabb.center(), camera->position());
    float lod = std::max(1.0f, -distance * 4e-2f + 4.0f);
    std::copy(
        this->blade_data_[node].data(),
        this->blade_data_[node].data() + (uint32_t)(node->num_triangles * lod),
        this->blade_data_for_gpu_.data() + num_blades);
    num_blades += (uint32_t)(node->num_triangles * lod);
  });

  // draw blades with calculated transforms
  blade_->shader()->Use();
  light_sources->Set(blade_->shader(), false);
  // bind vbo to glsl
  glBindVertexArray(blade_->vao());
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_blades * sizeof(InstancingData),
                  &blade_data_for_gpu_[0]);
  for (int i = 0; i < 4; i++) {
    uint32_t location = 3 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE,
                          sizeof(InstancingData),
                          (void*)(i * sizeof(glm::vec4)));
    glVertexAttribDivisor(location, 1);
  }
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(InstancingData),
                        (void*)offsetof(InstancingData, position));
  glVertexAttribDivisor(7, 1);
  glEnableVertexAttribArray(8);
  glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(InstancingData),
                        (void*)offsetof(InstancingData, tex_coord));
  glVertexAttribDivisor(8, 1);

  // set uniforms
  blade_->shader()->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  blade_->shader()->SetUniform<glm::mat4>("uProjectionMatrix",
                                          camera->projection_matrix());
  blade_->shader()->SetUniform<glm::vec3>("uCameraPosition",
                                          camera->position());
  blade_->shader()->SetUniform<glm::vec2>("uWindFrequency", glm::vec2(0.1));
  blade_->shader()->SetUniform<float>("uTime", (float)time);
  blade_->shader()->SetUniformSampler("uDistortionTexture", distortion_texture_,
                                      0);

  // draw blades
  glDrawElementsInstanced(GL_TRIANGLES, blade_->indices_size(), GL_UNSIGNED_INT,
                          0, num_blades);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}