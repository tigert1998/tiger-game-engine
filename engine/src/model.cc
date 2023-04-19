
#define NOMINMAX

#include "model.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glog/logging.h>

#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "utils.h"

using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace Assimp;
using namespace glm;

std::shared_ptr<Shader> Model::kShader = nullptr;

Model::Model(const std::string &path,
             const std::vector<std::string> &filtered_node_names)
    : directory_path_(ParentPath(ParentPath(path))),
      filtered_node_names_(filtered_node_names) {
  LOG(INFO) << "loading model at: \"" << path << "\"";
  scene_ = aiImportFile(path.c_str(), aiProcess_GlobalScale |
                                          aiProcess_CalcTangentSpace |
                                          aiProcess_Triangulate);
  if (kShader == nullptr) {
    kShader.reset(new Shader(Model::kVsSource, Model::kFsSource));
  }
  shader_ptr_ = kShader;
  glGenBuffers(1, &vbo_);

  animation_channel_map_.clear();
  for (int i = 0; i < scene_->mNumAnimations; i++) {
    auto animation = scene_->mAnimations[i];
    for (int j = 0; j < animation->mNumChannels; j++) {
      auto channel = animation->mChannels[j];
      animation_channel_map_[pair<uint32_t, string>(
          i, channel->mNodeName.C_Str())] = j;
    }
  }

  mesh_ptrs_.resize(scene_->mNumMeshes);
  LOG(INFO) << "#meshes: " << mesh_ptrs_.size();
  LOG(INFO) << "#animations: " << scene_->mNumAnimations;
  min_ = vec3(INFINITY);
  max_ = -min_;
  RecursivelyInitNodes(scene_->mRootNode, mat4(1));
  bone_matrices_.resize(bone_namer_.total());

  LOG(INFO) << "min: (" << min_.x << ", " << min_.y << ", " << min_.z << "), "
            << "max: (" << max_.x << ", " << max_.y << ", " << max_.z << ")";
}

Model::Model(const std::string &path)
    : Model(path, std::vector<std::string>({})) {}

Model::~Model() {
  aiReleaseImport(scene_);
  glDeleteBuffers(1, &vbo_);
}

bool Model::NodeShouldBeFiltered(const std::string &name) {
  for (int i = 0; i < filtered_node_names_.size(); i++) {
    if (name == filtered_node_names_[i]) return true;
  }
  return false;
}

void Model::RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform) {
  auto transform =
      parent_transform * Mat4FromAimatrix4x4(node->mTransformation);

  if (!NodeShouldBeFiltered(node->mName.C_Str())) {
    LOG(INFO) << "initializing node \"" << node->mName.C_Str() << "\"";
    for (int i = 0; i < node->mNumMeshes; i++) {
      int id = node->mMeshes[i];
      if (mesh_ptrs_[id] == nullptr) {
        auto mesh = scene_->mMeshes[id];
        try {
          mesh_ptrs_[id] = make_shared<Mesh>(directory_path_, mesh, scene_,
                                             bone_namer_, bone_offsets_);
          max_ = (glm::max)(max_, mesh_ptrs_[id]->max());
          min_ = (glm::min)(min_, mesh_ptrs_[id]->min());
        } catch (std::exception &e) {
          mesh_ptrs_[id] = nullptr;
          LOG(WARNING) << "not loading mesh \"" << mesh->mName.C_Str()
                       << "\" because an exception is thrown: " << e.what();
        }
      }
      if (mesh_ptrs_[id] != nullptr) {
        mesh_ptrs_[id]->AppendTransform(transform);
      }
    }
  }
  for (int i = 0; i < node->mNumChildren; i++) {
    RecursivelyInitNodes(node->mChildren[i], transform);
  }
}

glm::mat4 Model::InterpolateTranslationMatrix(aiVectorKey *keys, uint32_t n,
                                              double ticks) {
  static auto mat4_from_aivector3d = [](aiVector3D vector) -> mat4 {
    return translate(mat4(1), vec3(vector.x, vector.y, vector.z));
  };
  if (n == 0) return mat4(1);
  if (n == 1) return mat4_from_aivector3d(keys->mValue);
  if (ticks <= keys[0].mTime) return mat4_from_aivector3d(keys[0].mValue);
  if (keys[n - 1].mTime <= ticks)
    return mat4_from_aivector3d(keys[n - 1].mValue);

  aiVectorKey anchor;
  anchor.mTime = ticks;
  auto right_ptr = std::upper_bound(
      keys, keys + n, anchor, [](const aiVectorKey &a, const aiVectorKey &b) {
        return a.mTime < b.mTime;
      });
  auto left_ptr = right_ptr - 1;

  float factor =
      (ticks - left_ptr->mTime) / (right_ptr->mTime - left_ptr->mTime);
  return mat4_from_aivector3d(left_ptr->mValue * (1.0f - factor) +
                              right_ptr->mValue * factor);
}

glm::mat4 Model::InterpolateRotationMatrix(aiQuatKey *keys, uint32_t n,
                                           double ticks) {
  static auto mat4_from_aiquaternion = [](aiQuaternion quaternion) -> mat4 {
    auto rotation_matrix = quaternion.GetMatrix();
    mat4 res(1);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) res[j][i] = rotation_matrix[i][j];
    return res;
  };
  if (n == 0) return mat4(1);
  if (n == 1) return mat4_from_aiquaternion(keys->mValue);
  if (ticks <= keys[0].mTime) return mat4_from_aiquaternion(keys[0].mValue);
  if (keys[n - 1].mTime <= ticks)
    return mat4_from_aiquaternion(keys[n - 1].mValue);

  aiQuatKey anchor;
  anchor.mTime = ticks;
  auto right_ptr = std::upper_bound(
      keys, keys + n, anchor,
      [](const aiQuatKey &a, const aiQuatKey &b) { return a.mTime < b.mTime; });
  auto left_ptr = right_ptr - 1;

  double factor =
      (ticks - left_ptr->mTime) / (right_ptr->mTime - left_ptr->mTime);
  aiQuaternion out;
  aiQuaternion::Interpolate(out, left_ptr->mValue, right_ptr->mValue, factor);
  return mat4_from_aiquaternion(out);
}

glm::mat4 Model::InterpolateScalingMatrix(aiVectorKey *keys, uint32_t n,
                                          double ticks) {
  static auto mat4_from_aivector3d = [](aiVector3D vector) -> mat4 {
    return scale(mat4(1), vec3(vector.x, vector.y, vector.z));
  };
  if (n == 0) return mat4(1);
  if (n == 1) return mat4_from_aivector3d(keys->mValue);
  if (ticks <= keys[0].mTime) return mat4_from_aivector3d(keys[0].mValue);
  if (keys[n - 1].mTime <= ticks)
    return mat4_from_aivector3d(keys[n - 1].mValue);

  aiVectorKey anchor;
  anchor.mTime = ticks;
  auto right_ptr = std::upper_bound(
      keys, keys + n, anchor, [](const aiVectorKey &a, const aiVectorKey &b) {
        return a.mTime < b.mTime;
      });
  auto left_ptr = right_ptr - 1;

  float factor =
      (ticks - left_ptr->mTime) / (right_ptr->mTime - left_ptr->mTime);
  return mat4_from_aivector3d(left_ptr->mValue * (1.0f - factor) +
                              right_ptr->mValue * factor);
}

int Model::NumAnimations() const { return scene_->mNumAnimations; }

void Model::RecursivelyUpdateBoneMatrices(int animation_id, aiNode *node,
                                          glm::mat4 transform, double ticks) {
  string node_name = node->mName.C_Str();
  auto animation = scene_->mAnimations[animation_id];
  mat4 current_transform;
  if (animation_channel_map_.count(
          pair<uint32_t, string>(animation_id, node_name))) {
    uint32_t channel_id =
        animation_channel_map_[pair<uint32_t, string>(animation_id, node_name)];
    auto channel = animation->mChannels[channel_id];

    // translation matrix
    mat4 translation_matrix = InterpolateTranslationMatrix(
        channel->mPositionKeys, channel->mNumPositionKeys, ticks);
    // rotation matrix
    mat4 rotation_matrix = InterpolateRotationMatrix(
        channel->mRotationKeys, channel->mNumRotationKeys, ticks);
    // scaling matrix
    mat4 scaling_matrix = InterpolateScalingMatrix(
        channel->mScalingKeys, channel->mNumScalingKeys, ticks);

    current_transform = translation_matrix * rotation_matrix * scaling_matrix;
  } else {
    current_transform = Mat4FromAimatrix4x4(node->mTransformation);
  }
  if (bone_namer_.map().count(node_name)) {
    uint32_t i = bone_namer_.map()[node_name];
    bone_matrices_[i] = transform * current_transform * bone_offsets_[i];
  }
  for (int i = 0; i < node->mNumChildren; i++) {
    RecursivelyUpdateBoneMatrices(animation_id, node->mChildren[i],
                                  transform * current_transform, ticks);
  }
}

void Model::Draw(uint32_t animation_id, double time, Camera *camera_ptr,
                 LightSources *light_sources, mat4 model_matrix,
                 vec4 clip_plane) {
  RecursivelyUpdateBoneMatrices(
      animation_id, scene_->mRootNode, mat4(1),
      time * scene_->mAnimations[animation_id]->mTicksPerSecond);
  InternalDraw(true, camera_ptr, light_sources,
               std::vector<glm::mat4>{model_matrix}, clip_plane, false);
}

void Model::Draw(Camera *camera_ptr, LightSources *light_sources,
                 const std::vector<glm::mat4> &model_matrices,
                 vec4 clip_plane) {
  InternalDraw(false, camera_ptr, light_sources, model_matrices, clip_plane,
               false);
}

void Model::Draw(Camera *camera_ptr, LightSources *light_sources,
                 mat4 model_matrix) {
  InternalDraw(false, camera_ptr, light_sources,
               std::vector<glm::mat4>{model_matrix}, glm::vec4(0), false);
}

void Model::InternalDraw(bool animated, Camera *camera_ptr,
                         LightSources *light_sources,
                         const std::vector<glm::mat4> &model_matrices,
                         glm::vec4 clip_plane, bool sort_meshes) {
  shader_ptr_->Use();
  if (light_sources != nullptr) {
    light_sources->Set(shader_ptr_.get());
  }
  shader_ptr_->SetUniform<vec4>("uClipPlane", clip_plane);
  shader_ptr_->SetUniform<mat4>("uViewMatrix", camera_ptr->view_matrix());
  shader_ptr_->SetUniform<mat4>("uProjectionMatrix",
                                camera_ptr->projection_matrix());
  shader_ptr_->SetUniform<vector<mat4>>("uBoneMatrices", bone_matrices_);
  shader_ptr_->SetUniform<vec3>("uCameraPosition", camera_ptr->position());
  shader_ptr_->SetUniform<int32_t>("uDefaultShading", default_shading_);

  auto draw_mesh = [&](Mesh *mesh_ptr, Shader *shader_ptr) {
    glBindVertexArray(mesh_ptr->vao());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, model_matrices.size() * sizeof(glm::mat4),
                 model_matrices.data(), GL_DYNAMIC_DRAW);
    for (int i = 0; i < 4; i++) {
      uint32_t location = 9 + i;
      glEnableVertexAttribArray(location);
      glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void *)(i * sizeof(glm::vec4)));
      glVertexAttribDivisor(location, 1);
    }

    mesh_ptr->Draw(shader_ptr, model_matrices.size());
  };

  if (sort_meshes) {
    // sort meshes does not fit well to instancing
    std::vector<std::pair<int, glm::vec3>> order;
    order.reserve(mesh_ptrs_.size());
    for (int i = 0; i < mesh_ptrs_.size(); i++)
      if (mesh_ptrs_[i] != nullptr) {
        glm::vec3 pos =
            camera_ptr->view_matrix() * model_matrices[0] *
            vec4(mesh_ptrs_[i]->center(animated ? &bone_matrices_[0] : nullptr),
                 1);
        order.emplace_back(i, pos);
      }
    std::sort(order.begin(), order.end(), [](const auto &x, const auto &y) {
      return x.second.z < y.second.z;
    });

    for (int i = 0; i < order.size(); i++) {
      shader_ptr_->SetUniform<int32_t>("uAnimated", animated);
      draw_mesh(mesh_ptrs_[order[i].first].get(), shader_ptr_.get());
    }
  } else {
    for (int i = 0; i < mesh_ptrs_.size(); i++) {
      if (mesh_ptrs_[i] == nullptr) continue;
      shader_ptr_->SetUniform<int32_t>("uAnimated", animated);
      draw_mesh(mesh_ptrs_[i].get(), shader_ptr_.get());
    }
  }
}

void Model::set_default_shading(bool default_shading) {
  default_shading_ = default_shading;
}

const std::string Model::kVsSource = R"(
#version 410 core

const int MAX_BONES = 100;
const int MAX_INSTANCES = 20;

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in ivec4 aBoneIDs0;
layout (location = 4) in ivec4 aBoneIDs1;
layout (location = 5) in ivec4 aBoneIDs2;
layout (location = 6) in vec4 aBoneWeights0;
layout (location = 7) in vec4 aBoneWeights1;
layout (location = 8) in vec4 aBoneWeights2;
layout (location = 9) in mat4 aModelMatrix;

out vec3 vPosition;
out vec2 vTexCoord;
out vec3 vNormal;

uniform bool uAnimated;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 uBoneMatrices[MAX_BONES];
uniform mat4 uTransform;
uniform vec4 uClipPlane;

mat4 CalcBoneMatrix() {
    mat4 boneMatrix = mat4(0);
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs0[i] < 0) return boneMatrix;
        boneMatrix += uBoneMatrices[aBoneIDs0[i]] * aBoneWeights0[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs1[i] < 0) return boneMatrix;
        boneMatrix += uBoneMatrices[aBoneIDs1[i]] * aBoneWeights1[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs2[i] < 0) return boneMatrix;
        boneMatrix += uBoneMatrices[aBoneIDs2[i]] * aBoneWeights2[i];
    }
    return boneMatrix;
}

void main() {
    mat4 transform;
    if (uAnimated) {
      transform = CalcBoneMatrix();
    } else {
      transform = uTransform;
    }
    gl_Position = uProjectionMatrix * uViewMatrix * aModelMatrix * transform * vec4(aPosition, 1);
    vPosition = vec3(aModelMatrix * transform * vec4(aPosition, 1));
    vTexCoord = aTexCoord;
    vNormal = vec3(aModelMatrix * transform * vec4(aNormal, 0));
    gl_ClipDistance[0] = dot(vec4(vPosition, 1), uClipPlane);
}
)";

const std::string Model::kFsSource = R"(
#version 410 core

const float zero = 0.00000001;

uniform vec3 uCameraPosition;

in vec3 vPosition;
in vec2 vTexCoord;
in vec3 vNormal;

#define REG_TEX(name) \
    uniform bool u##name##Enabled;      \
    uniform sampler2D u##name##Texture;

REG_TEX(Ambient)
REG_TEX(Diffuse)
REG_TEX(Emissive)
REG_TEX(BaseColor)

#undef REG_TEX
)" + LightSources::kFsSource + R"(
uniform bool uDefaultShading;

out vec4 fragColor;

vec3 defaultShading() {
    vec3 raw = vec3(0.9608f, 0.6784f, 0.2588f);
    return calcPhoneLighting(
        vec3(1), vec3(1), vec3(1),
        vNormal, uCameraPosition, vPosition,
        20,
        raw, raw, raw
    );
}

void main() {
    if (uDefaultShading) {
        fragColor = vec4(defaultShading(), 1);
        return;
    }
    float alpha = 1.0f;

    vec3 ambientColor = vec3(0);
    vec3 diffuseColor = vec3(0);
    vec3 specularColor = vec3(0);

    if (uAmbientEnabled) {
        ambientColor = texture(uAmbientTexture, vTexCoord).rgb;
    }
    if (uDiffuseEnabled) {
        vec4 sampled = texture(uDiffuseTexture, vTexCoord);
        diffuseColor = sampled.rgb;
        alpha = sampled.a;
    } else if (uBaseColorEnabled) {
        vec4 sampled = texture(uBaseColorTexture, vTexCoord);
        diffuseColor = sampled.rgb;
        alpha = sampled.a;
    }
    vec3 color = calcPhoneLighting(
        vec3(1), vec3(1), vec3(1),
        vNormal, uCameraPosition, vPosition,
        20,
        ambientColor, diffuseColor, specularColor
    );

    if (uEmissiveEnabled) {
        color += texture(uEmissiveTexture, vTexCoord).rgb;
    }

    fragColor = vec4(color, alpha);
}
)";