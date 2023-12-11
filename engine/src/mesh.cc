
#include "mesh.h"

#include <glad/glad.h>
#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <vector>

#include "texture_manager.h"
#include "utils.h"

using glm::vec2;
using glm::vec3;

Namer::Namer() { Clear(); }

void Namer::Clear() {
  map_.clear();
  total_ = 0;
}

uint32_t Namer::Name(const std::string &name) {
  if (map_.count(name)) return map_[name];
  return map_[name] = total_++;
}

uint32_t Namer::total() const { return total_; }

std::map<std::string, uint32_t> &Namer::map() { return map_; }

constexpr int kMaxBonesPerVertex = 12;

using VertexWithBones = Vertex<kMaxBonesPerVertex>;

Mesh::Mesh(const std::string &directory_path, aiMesh *mesh,
           const aiScene *scene, Namer &bone_namer,
           std::vector<glm::mat4> &bone_offsets, bool flip_y) {
#define REGISTER(name) \
  textures_[#name] = TextureRecord(false, Texture(), -1, -1, glm::vec3(0))
  REGISTER(DIFFUSE);
  REGISTER(AMBIENT);
  REGISTER(SPECULAR);
  REGISTER(NORMALS);
  REGISTER(METALNESS);
  REGISTER(DIFFUSE_ROUGHNESS);
  REGISTER(AMBIENT_OCCLUSION);
#undef REGISTER

  std::vector<VertexWithBones> vertices;
  std::vector<uint32_t> indices;
  name_ = mesh->mName.C_Str();

  auto path = directory_path;
  {
    auto material = scene->mMaterials[mesh->mMaterialIndex];
    aiShadingMode shading_mode;
    if (material->Get(AI_MATKEY_SHADING_MODEL, shading_mode) == AI_SUCCESS) {
      LOG(INFO) << "\"" << name_ << "\" shading mode: " << shading_mode;
    }
    {
      aiColor3D color;
      float value;
      if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_.ka = glm::vec3(color.r, color.g, color.b);
      if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_.kd = glm::vec3(color.r, color.g, color.b);
      if (material->Get(AI_MATKEY_SHININESS_STRENGTH, value) != AI_SUCCESS) {
        value = 1;
      }
      if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_.ks = glm::vec3(color.r, color.g, color.b) * value;
      if (material->Get(AI_MATKEY_SHININESS, value) != AI_SUCCESS) {
        value = 0;
      }
      material_.shininess = value;
    }
    aiString material_texture_path;
#define INTERNAL_ADD_TEXTURE(name)                                          \
  do {                                                                      \
    textures_[#name].enabled = true;                                        \
    material->GetTexture(aiTextureType_##name, 0, &material_texture_path);  \
    auto item = path + "/" + std::string(material_texture_path.C_Str());    \
    textures_[#name].texture = Texture(                                     \
        item, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, {}, true);     \
    bool is_dds = ToLower(item.substr(item.size() - 4)) == ".dds";          \
    CHECK((is_dds && flip_y) || (!is_dds && !flip_y))                       \
        << "Currently, DDS image format does not support flipping. So you " \
           "must flip the UV coordinates by passing flip_y = true when "    \
           "initializing Model.";                                           \
  } while (0)
#define TRY_ADD_TEXTURE(name)                                     \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {     \
    CHECK_EQ(material->GetTextureCount(aiTextureType_##name), 1); \
    INTERNAL_ADD_TEXTURE(name);                                   \
  }
#define TRY_ADD_TEXTURE_WITH_BASE_COLOR(name)                              \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {              \
    CHECK_EQ(material->GetTextureCount(aiTextureType_##name), 1);          \
    INTERNAL_ADD_TEXTURE(name);                                            \
    material->Get(AI_MATKEY_TEXBLEND_##name(0), textures_[#name].blend);   \
    material->Get(AI_MATKEY_TEXOP_##name(0), textures_[#name].op);         \
    aiColor3D color(0.f, 0.f, 0.f);                                        \
    material->Get(AI_MATKEY_COLOR_##name, color);                          \
    textures_[#name].base_color = glm::vec3(color[0], color[1], color[2]); \
  }
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(DIFFUSE);
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(AMBIENT);
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(SPECULAR);
    TRY_ADD_TEXTURE(NORMALS);
    TRY_ADD_TEXTURE(METALNESS);
    TRY_ADD_TEXTURE(DIFFUSE_ROUGHNESS);
    TRY_ADD_TEXTURE(AMBIENT_OCCLUSION);
#undef INTERNAL_ADD_TEXTURE
#undef TRY_ADD_TEXTURE
#undef TRY_ADD_TEXTURE_WITH_BASE_COLOR

    bind_metalness_and_diffuse_roughness_ = [material]() {
      if (material->GetTextureCount(aiTextureType_METALNESS) >= 1 &&
          material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) >= 1) {
        aiString metalness_path;
        material->GetTexture(aiTextureType_METALNESS, 0, &metalness_path);
        aiString diffuse_roughness_path;
        material->GetTexture(aiTextureType_METALNESS, 0,
                             &diffuse_roughness_path);
        return metalness_path == diffuse_roughness_path;
      } else {
        return false;
      }
    }();
  }

  LOG(INFO) << "\"" << name() << "\": #vertices: " << mesh->mNumVertices
            << ", #faces: " << mesh->mNumFaces << ", has tex coords? "
            << std::boolalpha << mesh->HasTextureCoords(0) << ", has normals? "
            << mesh->HasNormals();

  vertices.reserve(mesh->mNumVertices);
  indices.reserve(mesh->mNumFaces * 3);
  center_ = glm::vec3(0);

  min_ = vec3(INFINITY);
  max_ = -min_;

  for (int i = 0; i < mesh->mNumVertices; i++) {
    auto vertex = VertexWithBones();
    vertex.position =
        vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
    min_ = (glm::min)(vertex.position, min_);
    max_ = (glm::max)(vertex.position, max_);
    center_ += vertex.position;
    if (mesh->HasTextureCoords(0)) {
      vertex.tex_coord =
          vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    }
    if (mesh->HasNormals()) {
      vertex.normal =
          vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
    }
    if (mesh->HasTangentsAndBitangents()) {
      vertex.tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y,
                            mesh->mTangents[i].z);
    }
    vertices.push_back(vertex);
  }
  center_ /= mesh->mNumVertices;

  LOG(INFO) << "\"" << name() << "\": "
            << "center: (" << center_.x << ", " << center_.y << ", "
            << center_.z << ")";

  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }
  indices_size_ = (uint32_t)indices.size();

  for (int i = 0; i < mesh->mNumBones; i++) {
    auto bone = mesh->mBones[i];
    auto id = bone_namer.Name(bone->mName.C_Str());

    bone_offsets.resize((std::max)(id + 1, (uint32_t)bone_offsets.size()));
    bone_offsets[id] = Mat4FromAimatrix4x4(bone->mOffsetMatrix);

    for (int j = 0; j < bone->mNumWeights; j++) {
      auto weight = bone->mWeights[j];
      vertices[weight.mVertexId].AddBone(id, weight.mWeight);
      has_bone_ = true;
    }
  }

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);

  glBindVertexArray(vao_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VertexWithBones) * vertices.size(),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexWithBones),
                        (void *)offsetof(VertexWithBones, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexWithBones),
                        (void *)offsetof(VertexWithBones, tex_coord));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexWithBones),
                        (void *)offsetof(VertexWithBones, normal));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT, false, sizeof(VertexWithBones),
                        (void *)offsetof(VertexWithBones, tangent));

  for (int i = 0; i < kMaxBonesPerVertex / 4; i++) {
    int location = 4 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribIPointer(
        location, 4, GL_INT, sizeof(VertexWithBones),
        (void *)(offsetof(VertexWithBones, bone_ids) + i * 4 * sizeof(int)));
  }
  for (int i = 0; i < kMaxBonesPerVertex / 4; i++) {
    int location = 4 + kMaxBonesPerVertex / 4 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 4, GL_FLOAT, false, sizeof(VertexWithBones),
                          (void *)(offsetof(VertexWithBones, bone_weights) +
                                   i * 4 * sizeof(float)));
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::AppendTransform(glm::mat4 transform) {
  transforms_.push_back(transform);
}

Mesh::~Mesh() {
  glDeleteVertexArrays(1, &vao_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &ebo_);
}

void Mesh::Draw(Shader *shader_ptr, int num_instances, bool shadow,
                int32_t sampler_offset) const {
  if (!has_bone_) {
    shader_ptr->SetUniform<int32_t>("uAnimated", 0);
  }

  if (!shadow) {
    for (auto iter = textures_.begin(); iter != textures_.end(); iter++) {
      bool enabled = iter->second.enabled;
      std::string name = SnakeToPascal(iter->first);
      name[0] = tolower(name[0]);  // e.g. "Ambient" -> "ambient"
      shader_ptr->SetUniform<int32_t>(
          std::string("uMaterial.") + name + "TextureEnabled", enabled);
      if (enabled) {
        shader_ptr->SetUniformSampler(
            std::string("uMaterial.") + name + "Texture", iter->second.texture,
            sampler_offset++);
      }
    }

    shader_ptr->SetUniform<glm::vec3>("uMaterial.ka", material_.ka);
    shader_ptr->SetUniform<glm::vec3>("uMaterial.kd", material_.kd);
    shader_ptr->SetUniform<glm::vec3>("uMaterial.ks", material_.ks);
    shader_ptr->SetUniform<float>("uMaterial.shininess", material_.shininess);
    shader_ptr->SetUniform<int32_t>(
        "uMaterial.bindMetalnessAndDiffuseRoughness",
        bind_metalness_and_diffuse_roughness_);
  }

  CHECK(transforms_.size() == 1);
  shader_ptr->SetUniform<glm::mat4>("uTransform", transforms_[0]);

  glBindVertexArray(vao_);
  glDrawElementsInstanced(GL_TRIANGLES, indices_size_, GL_UNSIGNED_INT, 0,
                          num_instances);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}