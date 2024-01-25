#include "mesh.h"

#include <glad/glad.h>
#include <glog/logging.h>
#include <meshoptimizer.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.h"
#include "vertex.h"

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

Mesh::Mesh(const std::string &directory_path, aiMesh *mesh,
           const aiScene *scene, Namer *bone_namer,
           std::vector<glm::mat4> *bone_offsets, bool flip_y,
           MultiDrawIndirect *multi_draw_indirect)
    : multi_draw_indirect_(multi_draw_indirect) {
#define REGISTER(name) \
  textures_.push_back( \
      TextureRecord(#name, false, Texture(), -1, -1, glm::vec3(0)))
  REGISTER(AMBIENT);
  REGISTER(DIFFUSE);
  REGISTER(SPECULAR);
  REGISTER(NORMALS);
  REGISTER(METALNESS);
  REGISTER(DIFFUSE_ROUGHNESS);
  REGISTER(AMBIENT_OCCLUSION);
#undef REGISTER

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
#define INTERNAL_ADD_TEXTURE(i, name, srgb)                                \
  do {                                                                     \
    CHECK(textures_[i].type == #name);                                     \
    textures_[i].enabled = true;                                           \
    material->GetTexture(aiTextureType_##name, 0, &material_texture_path); \
    auto item = path + "/" + std::string(material_texture_path.C_Str());   \
    textures_[i].texture =                                                 \
        Texture::LoadFromFS(item, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR,      \
                            GL_LINEAR, {}, true, !flip_y, srgb);           \
  } while (0)
#define TRY_ADD_TEXTURE(i, name, srgb)                            \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {     \
    CHECK_EQ(material->GetTextureCount(aiTextureType_##name), 1); \
    INTERNAL_ADD_TEXTURE(i, name, srgb);                          \
  }
#define TRY_ADD_TEXTURE_WITH_BASE_COLOR(i, name, srgb)                 \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {          \
    CHECK_EQ(material->GetTextureCount(aiTextureType_##name), 1);      \
    INTERNAL_ADD_TEXTURE(i, name, srgb);                               \
    material->Get(AI_MATKEY_TEXBLEND_##name(0), textures_[i].blend);   \
    material->Get(AI_MATKEY_TEXOP_##name(0), textures_[i].op);         \
    aiColor3D color(0.f, 0.f, 0.f);                                    \
    material->Get(AI_MATKEY_COLOR_##name, color);                      \
    textures_[i].base_color = glm::vec3(color[0], color[1], color[2]); \
  }
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(0, AMBIENT, false);
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(1, DIFFUSE, false);
    TRY_ADD_TEXTURE_WITH_BASE_COLOR(2, SPECULAR, false);
    TRY_ADD_TEXTURE(3, NORMALS, false);
    TRY_ADD_TEXTURE(4, METALNESS, false);
    TRY_ADD_TEXTURE(5, DIFFUSE_ROUGHNESS, false);
    TRY_ADD_TEXTURE(6, AMBIENT_OCCLUSION, false);
#undef INTERNAL_ADD_TEXTURE
#undef TRY_ADD_TEXTURE
#undef TRY_ADD_TEXTURE_WITH_BASE_COLOR
  }

  vertices_.reserve(mesh->mNumVertices);
  indices_.reserve(mesh->mNumFaces * 3);

  for (int i = 0; i < mesh->mNumVertices; i++) {
    auto vertex = VertexWithBones();
    vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                mesh->mVertices[i].z);
    if (mesh->HasTextureCoords(0)) {
      vertex.tex_coord =
          glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    }
    if (mesh->HasNormals()) {
      vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                                mesh->mNormals[i].z);
    }
    if (mesh->HasTangentsAndBitangents()) {
      vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y,
                                 mesh->mTangents[i].z);
    }
    vertices_.push_back(vertex);
  }

  indices_.resize(1);
  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices_[0].push_back(face.mIndices[j]);
  }

  for (int i = 0; i < mesh->mNumBones; i++) {
    auto bone = mesh->mBones[i];
    auto id = bone_namer->Name(bone->mName.C_Str());

    bone_offsets->resize((std::max)(id + 1, (uint32_t)bone_offsets->size()));
    bone_offsets->at(id) = Mat4FromAimatrix4x4(bone->mOffsetMatrix);

    for (int j = 0; j < bone->mNumWeights; j++) {
      auto weight = bone->mWeights[j];
      vertices_[weight.mVertexId].AddBone(id, weight.mWeight);
      has_bone_ = true;
    }
  }

  for (int i = 1; i <= 7; i++) {
    if (indices_[i - 1].size() <= 1024 * 3) break;
    indices_.push_back({});
    indices_[i].resize(indices_[i - 1].size());
    float threshold = 0.5f;
    size_t target_index_count = size_t(indices_[i - 1].size() * threshold);
    float target_error = 0.2;
    uint32_t options = 0;
    float lod_error;
    indices_[i].resize(meshopt_simplifySloppy(
        indices_[i].data(), indices_[i - 1].data(), indices_[i - 1].size(),
        glm::value_ptr(vertices_[0].position), vertices_.size(),
        sizeof(vertices_[0]), target_index_count, target_error, &lod_error));

    if (indices_[i].size() == indices_[i - 1].size()) {
      // meshopt stops early
      indices_.resize(i);
      break;
    }
  }

  std::string lod_log_str = "LOD: [";
  for (int i = 0; i < indices_.size(); i++) {
    lod_log_str += std::to_string(indices_[i].size());
    if (i < indices_.size() - 1) lod_log_str += ", ";
  }
  lod_log_str += "]";
  LOG(INFO) << "\"" << name_ << "\": #vertices: " << mesh->mNumVertices
            << ", #faces: " << mesh->mNumFaces << ", " << lod_log_str
            << ", has tex coords? " << std::boolalpha
            << mesh->HasTextureCoords(0) << ", has normals? "
            << mesh->HasNormals();
}

void Mesh::AppendTransform(glm::mat4 transform) {
  transforms_.push_back(transform);
}

void Mesh::SubmitToMultiDrawIndirect() {
  multi_draw_indirect_->Receive(vertices_, indices_[0], textures_, material_,
                                has_bone_, transforms_[0]);
}