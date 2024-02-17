#include "model.h"

#include <assimp/postprocess.h>
#include <fmt/core.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>

#include "utils.h"

namespace fs = std::filesystem;

Model::Model(const fs::path &path, MultiDrawIndirect *multi_draw_indirect,
             uint32_t item_count, bool flip_y, bool split_large_meshes)
    : directory_path_(path.parent_path()),
      flip_y_(flip_y),
      multi_draw_indirect_(multi_draw_indirect) {
  CompileShaders();

  fmt::print(stderr, "[info] loading model at: \"{}\"\n",
             (const char *)path.u8string().data());

  uint32_t flags = aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
                   aiProcess_Triangulate;
  if (flip_y_) flags |= aiProcess_FlipUVs;
  if (split_large_meshes) {
    flags |= aiProcess_SplitLargeMeshes;
    importer_.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 8192);
  }

  scene_ = importer_.ReadFile((const char *)path.u8string().data(), flags);
  if (scene_ == nullptr) {
    fmt::print(stderr, "[error] scene_ == nullptr\n");
    exit(1);
  }

  InitAnimationChannelMap();

  meshes_.resize(scene_->mNumMeshes);
  fmt::print(stderr, "[info] #meshes: {}\n", meshes_.size());
  fmt::print(stderr, "[info] #animations: {}\n", scene_->mNumAnimations);
  RecursivelyInitNodes(scene_->mRootNode, glm::mat4(1));

  bone_matrices_.resize(bone_namer_.total());

  multi_draw_indirect_->ModelBeginSubmission(this, item_count,
                                             bone_matrices_.size());
  for (int i = 0; i < meshes_.size(); i++) {
    if (meshes_[i] == nullptr) continue;
    meshes_[i]->SubmitToMultiDrawIndirect();
  }
  multi_draw_indirect_->ModelEndSubmission();
}

Model::~Model() {}

double Model::AnimationDurationInSeconds(int animation_id) const {
  if (animation_id < 0 || animation_id >= NumAnimations()) return 0;
  auto animation = scene_->mAnimations[animation_id];
  return animation->mDuration / animation->mTicksPerSecond;
}

void Model::RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform) {
  auto node_transform = Mat4FromAimatrix4x4(node->mTransformation);
  auto transform = parent_transform * node_transform;

  fmt::print(stderr, "[info] initializing node \"{}\"\n", node->mName.C_Str());
  for (int i = 0; i < node->mNumMeshes; i++) {
    int id = node->mMeshes[i];
    if (meshes_[id] == nullptr) {
      auto mesh = scene_->mMeshes[id];
      try {
        meshes_[id].reset(new Mesh(directory_path_, mesh, scene_, &bone_namer_,
                                   &bone_offsets_, flip_y_, transform,
                                   multi_draw_indirect_));
      } catch (std::exception &e) {
        fmt::print(
            stderr,
            "[error] not loading mesh \"{}\" because an exception is thrown: "
            "{}\n",
            (const char *)ToU8string(mesh->mName).data(), e.what());
        exit(1);
      }
    }
  }

  for (int i = 0; i < node->mNumChildren; i++) {
    RecursivelyInitNodes(node->mChildren[i], transform);
  }
}

glm::mat4 Model::InterpolateTranslationMatrix(aiVectorKey *keys, uint32_t n,
                                              double ticks) {
  static auto mat4_from_aivector3d = [](aiVector3D vector) -> glm::mat4 {
    return glm::translate(glm::mat4(1),
                          glm::vec3(vector.x, vector.y, vector.z));
  };
  if (n == 0) return glm::mat4(1);
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
  static auto mat4_from_aiquaternion =
      [](aiQuaternion quaternion) -> glm::mat4 {
    auto rotation_matrix = quaternion.GetMatrix();
    glm::mat4 res(1);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) res[j][i] = rotation_matrix[i][j];
    return res;
  };
  if (n == 0) return glm::mat4(1);
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
  static auto mat4_from_aivector3d = [](aiVector3D vector) -> glm::mat4 {
    return glm::scale(glm::mat4(1), glm::vec3(vector.x, vector.y, vector.z));
  };
  if (n == 0) return glm::mat4(1);
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
  std::string node_name = node->mName.C_Str();
  auto animation = scene_->mAnimations[animation_id];
  glm::mat4 current_transform;
  if (animation_channel_map_.count(
          std::pair<uint32_t, std::string>(animation_id, node_name))) {
    uint32_t channel_id =
        animation_channel_map_[std::pair<uint32_t, std::string>(animation_id,
                                                                node_name)];
    auto channel = animation->mChannels[channel_id];

    // translation matrix
    glm::mat4 translation_matrix = InterpolateTranslationMatrix(
        channel->mPositionKeys, channel->mNumPositionKeys, ticks);
    // rotation matrix
    glm::mat4 rotation_matrix = InterpolateRotationMatrix(
        channel->mRotationKeys, channel->mNumRotationKeys, ticks);
    // scaling matrix
    glm::mat4 scaling_matrix = InterpolateScalingMatrix(
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

void Model::InitAnimationChannelMap() {
  animation_channel_map_.clear();
  for (int i = 0; i < scene_->mNumAnimations; i++) {
    auto animation = scene_->mAnimations[i];
    for (int j = 0; j < animation->mNumChannels; j++) {
      auto channel = animation->mChannels[j];
      animation_channel_map_[{i, channel->mNodeName.C_Str()}] = j;
    }
  }
}

void Model::CompileShaders() {
  if (kShader == nullptr && kOITShader == nullptr && kShadowShader == nullptr &&
      kDeferredShadingShader == nullptr) {
    std::map<std::string, std::any> defines = {
        {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
        {"IS_SHADOW_PASS", std::any(false)},
        {"DIRECTIONAL_SHADOW_BINDING",
         std::any(DirectionalShadow::GLSL_BINDING)},
    };
    kShader.reset(new Shader("model/model.vert", "model/model.frag", defines));
    kOITShader.reset(new Shader("model/model.vert", "model/oit.frag", defines));
    kDeferredShadingShader.reset(
        new Shader("model/model.vert", "model/deferred_shading.frag", defines));

    defines["IS_SHADOW_PASS"] = std::any(true);
    kShadowShader.reset(new Shader(
        {
            {GL_VERTEX_SHADER, "model/model.vert"},
            {GL_GEOMETRY_SHADER, "model/shadow.geom"},
            {GL_FRAGMENT_SHADER, "model/shadow.frag"},
        },
        defines));
  }
}

std::unique_ptr<Shader> Model::kShader = nullptr;
std::unique_ptr<Shader> Model::kOITShader = nullptr;
std::unique_ptr<Shader> Model::kShadowShader = nullptr;
std::unique_ptr<Shader> Model::kDeferredShadingShader = nullptr;
