#include "mesh.h"

#include <fmt/core.h>
#include <glad/glad.h>
#include <meshoptimizer.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <tuple>

#include "utils.h"
#include "vertex.h"

namespace fs = std::filesystem;

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

Mesh::Mesh(const fs::path &directory_path, aiMesh *mesh, const aiScene *scene,
           Namer *bone_namer, std::vector<glm::mat4> *bone_offsets,
           std::map<fs::path, Texture> *textures_cache, bool flip_y,
           glm::mat4 transform)
    : transform_(transform) {
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
      fmt::print(stderr, "[info] \"{}\" shading mode: {}\n", name_,
                 static_cast<int32_t>(shading_mode));
    }
    {
      aiColor3D color;
      float value;
      if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_params_.ka = glm::vec3(color.r, color.g, color.b);
      if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_params_.kd = glm::vec3(color.r, color.g, color.b);
      if (material->Get(AI_MATKEY_SHININESS_STRENGTH, value) != AI_SUCCESS) {
        value = 1;
      }
      if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_params_.ks = glm::vec3(color.r, color.g, color.b) * value;
      if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) != AI_SUCCESS) {
        color = {0, 0, 0};
      }
      material_params_.ke = glm::vec3(color.r, color.g, color.b);
      if (material->Get(AI_MATKEY_SHININESS, value) != AI_SUCCESS) {
        value = 0;
      }
      material_params_.shininess = value;

      material_params_.albedo = material_params_.kd;
      material_params_.metallic = 0;
      material_params_.roughness = 0.25;
      material_params_.emission = glm::vec3(0);
    }
    aiString material_texture_path;
#define INTERNAL_ADD_TEXTURE(i, name, srgb)                                  \
  do {                                                                       \
    if (textures_[i].type != #name) {                                        \
      fmt::print(stderr, "[error] textures_[{}].type != " #name "\n", i);    \
      exit(1);                                                               \
    }                                                                        \
    textures_[i].enabled = true;                                             \
    material->GetTexture(aiTextureType_##name, 0, &material_texture_path);   \
    fs::path item = path / ToU8string(material_texture_path);                \
    textures_[i].texture = Texture::LoadFromFS(                              \
        textures_cache, item, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, \
        {}, true, !flip_y, srgb);                                            \
  } while (0)
#define TRY_ADD_TEXTURE(i, name, srgb)                                    \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {             \
    if (material->GetTextureCount(aiTextureType_##name) != 1) {           \
      fmt::print(stderr,                                                  \
                 "[error] material->GetTextureCount(aiTextureType_" #name \
                 ") != 1\n");                                             \
      exit(1);                                                            \
    }                                                                     \
    INTERNAL_ADD_TEXTURE(i, name, srgb);                                  \
  }
#define TRY_ADD_TEXTURE_WITH_BASE_COLOR(i, name, srgb)                    \
  if (material->GetTextureCount(aiTextureType_##name) >= 1) {             \
    if (material->GetTextureCount(aiTextureType_##name) != 1) {           \
      fmt::print(stderr,                                                  \
                 "[error] material->GetTextureCount(aiTextureType_" #name \
                 ") != 1\n");                                             \
      exit(1);                                                            \
    }                                                                     \
    INTERNAL_ADD_TEXTURE(i, name, srgb);                                  \
    material->Get(AI_MATKEY_TEXBLEND_##name(0), textures_[i].blend);      \
    material->Get(AI_MATKEY_TEXOP_##name(0), textures_[i].op);            \
    aiColor3D color(0.f, 0.f, 0.f);                                       \
    material->Get(AI_MATKEY_COLOR_##name, color);                         \
    textures_[i].base_color = glm::vec3(color[0], color[1], color[2]);    \
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

  MakeTexturesResidentOrNot(true);

  AddVerticesIndicesAndBones(mesh, bone_namer, bone_offsets);

  // generate AABB
  aabb_.min = glm::vec3((std::numeric_limits<float>::max)());
  aabb_.max = glm::vec3(std::numeric_limits<float>::lowest());
  if (!has_bone_) {
    for (int i = 0; i < indices_[0].size(); i++) {
      const auto &vertex = vertices_[indices_[0][i]];
      glm::vec3 position =
          glm::vec3(transform_ * glm::vec4(vertex.position, 1));
      aabb_.min = (glm::min)(position, aabb_.min);
      aabb_.max = (glm::max)(position, aabb_.max);
    }
  }

  for (int i = 1; i <= 7; i++) {
    if (indices_[i - 1].size() <= 1024) break;
    indices_.push_back({});
    indices_[i].resize(indices_[i - 1].size());
    float threshold = 0.5f;
    size_t target_index_count = size_t(indices_[i - 1].size() * threshold);
    float target_error = 0.2;
    uint32_t options = 0;
    float lod_error;
    indices_[i].resize(meshopt_simplify(
        indices_[i].data(), indices_[i - 1].data(), indices_[i - 1].size(),
        glm::value_ptr(vertices_[0].position), vertices_.size(),
        sizeof(vertices_[0]), target_index_count, target_error, options,
        &lod_error));

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
  fmt::print(
      stderr,
      "[info] \"{}\": #vertices: {}, #faces: {}, {}, has tex coords? {}, has "
      "normals? {}\n",
      name_, mesh->mNumVertices, mesh->mNumFaces, lod_log_str,
      mesh->HasTextureCoords(0), mesh->HasNormals());
}

MaterialParameters *Mesh::material_params() { return &material_params_; }

Mesh::~Mesh() { MakeTexturesResidentOrNot(false); }

void Mesh::AddVerticesIndicesAndBones(aiMesh *mesh, Namer *bone_namer,
                                      std::vector<glm::mat4> *bone_offsets) {
  for (int i = 0; i < mesh->mNumBones; i++) {
    auto bone = mesh->mBones[i];
    has_bone_ = bone->mNumWeights > 0;
    if (has_bone_) break;
  }

  vertices_.reserve(mesh->mNumVertices);
  indices_.reserve(mesh->mNumFaces * 3);

  using Tuple =
      std::tuple<float, float, float, float, float, float, float, float>;
  std::map<Tuple, uint32_t> vertex_dedup;
  std::map<uint32_t, uint32_t> index_remap;

  for (int i = 0; i < mesh->mNumVertices; i++) {
    glm::vec3 position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                   mesh->mVertices[i].z);
    glm::vec2 tex_coord(0);
    if (mesh->HasTextureCoords(0)) {
      tex_coord =
          glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    }
    glm::vec3 normal(0);
    if (mesh->HasNormals()) {
      normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                         mesh->mNormals[i].z);
    }
    glm::vec3 tangent(0);
    if (mesh->HasTangentsAndBitangents()) {
      tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y,
                          mesh->mTangents[i].z);
    }

    if (has_bone_) {
      // we don't deduplicate vertices for animated mesh
      index_remap[i] = i;
    } else {
      // vertex deduplication
      Tuple tp = {
          position.x,  position.y, position.z, tex_coord.x,
          tex_coord.y, normal.x,   normal.y,   normal.z,
      };
      if (vertex_dedup.count(tp)) {
        index_remap[i] = vertex_dedup[tp];
        continue;
      } else {
        index_remap[i] = vertex_dedup[tp] = vertices_.size();
      }
    }

    auto vertex = VertexWithBones();
    vertex.position = position;
    vertex.tex_coord = tex_coord;
    vertex.normal = normal;
    vertex.tangent = tangent;
    vertices_.push_back(vertex);
  }

  indices_.resize(1);
  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices_[0].push_back(index_remap[face.mIndices[j]]);
  }

  for (int i = 0; i < mesh->mNumBones; i++) {
    auto bone = mesh->mBones[i];
    auto id = bone_namer->Name(bone->mName.C_Str());

    bone_offsets->resize((std::max)(id + 1, (uint32_t)bone_offsets->size()));
    bone_offsets->at(id) = Mat4FromAimatrix4x4(bone->mOffsetMatrix);

    for (int j = 0; j < bone->mNumWeights; j++) {
      auto weight = bone->mWeights[j];
      vertices_[index_remap[weight.mVertexId]].AddBone(id, weight.mWeight);
    }
  }
}

void Mesh::SubmitToMultiDrawIndirect(MultiDrawIndirect *multi_draw_indirect) {
  multi_draw_indirect->Receive(vertices_, indices_, textures_, material_params_,
                               has_bone_, transform_, aabb_);
}

void Mesh::MakeTexturesResidentOrNot(bool resident) {
  for (int i = 0; i < textures_.size(); i++) {
    if (!textures_[i].enabled) continue;
    if (textures_[i].texture.has_ownership()) {
      if (resident)
        textures_[i].texture.MakeResident();
      else
        textures_[i].texture.MakeNonResident();
    }
  }
  CHECK_OPENGL_ERROR();
}
