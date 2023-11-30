
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

constexpr int MAX_BONES = 170;  // please change the shader's MAX_BONES as well

std::shared_ptr<Shader> Model::kShader = nullptr, Model::kOITShader = nullptr,
                        Model::kShadowShader = nullptr;

Model::Model(const std::string &path, OITRenderQuad *oit_render_quad)
    : directory_path_(ParentPath(path)), oit_render_quad_(oit_render_quad) {
  LOG(INFO) << "loading model at: \"" << path << "\"";
  scene_ = aiImportFile(path.c_str(), aiProcess_GlobalScale |
                                          aiProcess_CalcTangentSpace |
                                          aiProcess_Triangulate);

  if (kShader == nullptr && kOITShader == nullptr && kShadowShader == nullptr) {
    kShader = std::shared_ptr<Shader>(
        new Shader(Model::kVsSource, Model::kFsSource + Model::kFsMainSource));
    kOITShader = std::shared_ptr<Shader>(new Shader(
        Model::kVsSource, Model::kFsSource + Model::kFsOITMainSource));
    kShadowShader = std::shared_ptr<Shader>(new Shader(
        Model::kVsSource, Model::kFsSource + Model::kFsShadowMainSource));
  }
  bool enable_oit = oit_render_quad != nullptr;
  shader_ptr_ = enable_oit ? kOITShader : kShader;
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

  if (bone_namer_.total() > MAX_BONES) {
    throw MaxBoneExceededError();
  }
  bone_matrices_.resize(bone_namer_.total());

  LOG(INFO) << "min: (" << min_.x << ", " << min_.y << ", " << min_.z << "), "
            << "max: (" << max_.x << ", " << max_.y << ", " << max_.z << ")";
}

Model::~Model() {
  aiReleaseImport(scene_);
  glDeleteBuffers(1, &vbo_);
}

void Model::RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform) {
  auto node_transform = Mat4FromAimatrix4x4(node->mTransformation);
  auto transform = parent_transform * node_transform;

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
                 LightSources *light_sources, ShadowSources *shadow_sources,
                 mat4 model_matrix, vec4 clip_plane) {
  RecursivelyUpdateBoneMatrices(
      animation_id, scene_->mRootNode, mat4(1),
      time * scene_->mAnimations[animation_id]->mTicksPerSecond);
  InternalDraw(true, camera_ptr, light_sources, shadow_sources,
               std::vector<glm::mat4>{model_matrix}, clip_plane);
}

void Model::Draw(Camera *camera_ptr, LightSources *light_sources,
                 ShadowSources *shadow_sources,
                 const std::vector<glm::mat4> &model_matrices,
                 vec4 clip_plane) {
  InternalDraw(false, camera_ptr, light_sources, shadow_sources, model_matrices,
               clip_plane);
}

void Model::Draw(Camera *camera_ptr, LightSources *light_sources,
                 ShadowSources *shadow_sources, mat4 model_matrix) {
  InternalDraw(false, camera_ptr, light_sources, shadow_sources,
               std::vector<glm::mat4>{model_matrix}, glm::vec4(0));
}

void Model::DrawDepthForShadow(Shadow *shadow, glm::mat4 model_matrix) {
  InternalDrawDepthForShadow(false, shadow, {model_matrix});
}

void Model::DrawDepthForShadow(Shadow *shadow,
                               const std::vector<glm::mat4> &model_matrices) {
  InternalDrawDepthForShadow(false, shadow, model_matrices);
}

void Model::DrawDepthForShadow(uint32_t animation_id, double time,
                               Shadow *shadow, glm::mat4 model_matrix) {
  RecursivelyUpdateBoneMatrices(
      animation_id, scene_->mRootNode, mat4(1),
      time * scene_->mAnimations[animation_id]->mTicksPerSecond);
  InternalDrawDepthForShadow(true, shadow, {model_matrix});
}

void Model::DrawMesh(Mesh *mesh_ptr, Shader *shader_ptr,
                     const std::vector<glm::mat4> &model_matrices, bool shadow,
                     int32_t sampler_offset) {
  glBindVertexArray(mesh_ptr->vao());
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, model_matrices.size() * sizeof(glm::mat4),
               model_matrices.data(), GL_DYNAMIC_DRAW);
  for (int i = 0; i < 4; i++) {
    uint32_t location = 10 + i;
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void *)(i * sizeof(glm::vec4)));
    glVertexAttribDivisor(location, 1);
  }

  mesh_ptr->Draw(shader_ptr, model_matrices.size(), shadow, sampler_offset);
}

void Model::InternalDraw(bool animated, Camera *camera_ptr,
                         LightSources *light_sources,
                         ShadowSources *shadow_sources,
                         const std::vector<glm::mat4> &model_matrices,
                         glm::vec4 clip_plane) {
  int32_t num_samplers = 0;
  shader_ptr_->Use();
  if (oit_render_quad_ != nullptr) {
    oit_render_quad_->Set(shader_ptr_.get());
  }
  light_sources->Set(shader_ptr_.get(), false);
  shadow_sources->Set(shader_ptr_.get(), &num_samplers);
  shader_ptr_->SetUniform<vec4>("uClipPlane", clip_plane);
  shader_ptr_->SetUniform<mat4>("uViewMatrix", camera_ptr->view_matrix());
  shader_ptr_->SetUniform<mat4>("uProjectionMatrix",
                                camera_ptr->projection_matrix());
  shader_ptr_->SetUniform<vector<mat4>>("uBoneMatrices", bone_matrices_);
  shader_ptr_->SetUniform<vec3>("uCameraPosition", camera_ptr->position());
  shader_ptr_->SetUniform<int32_t>("uDefaultShading", default_shading_);

  for (int i = 0; i < mesh_ptrs_.size(); i++) {
    if (mesh_ptrs_[i] == nullptr) continue;
    shader_ptr_->SetUniform<int32_t>("uAnimated", animated);
    DrawMesh(mesh_ptrs_[i].get(), shader_ptr_.get(), model_matrices, false,
             num_samplers);
  }
}

void Model::InternalDrawDepthForShadow(
    bool animated, Shadow *shadow,
    const std::vector<glm::mat4> &model_matrices) {
  kShadowShader->Use();
  kShadowShader->SetUniform<vec4>("uClipPlane", glm::vec4(0));
  shadow->SetForDepthPass(kShadowShader.get());

  for (int i = 0; i < mesh_ptrs_.size(); i++) {
    if (mesh_ptrs_[i] == nullptr) continue;
    kShadowShader->SetUniform<int32_t>("uAnimated", animated);
    DrawMesh(mesh_ptrs_[i].get(), kShadowShader.get(), model_matrices, true, 0);
  }
}

void Model::set_default_shading(bool default_shading) {
  default_shading_ = default_shading;
}

const std::string Model::kVsSource = R"(
#version 410 core

const int MAX_BONES = 170;
const int MAX_INSTANCES = 20;
const int MAX_DIRECTIONAL_SHADOWS = 1;

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in ivec4 aBoneIDs0;
layout (location = 5) in ivec4 aBoneIDs1;
layout (location = 6) in ivec4 aBoneIDs2;
layout (location = 7) in vec4 aBoneWeights0;
layout (location = 8) in vec4 aBoneWeights1;
layout (location = 9) in vec4 aBoneWeights2;
layout (location = 10) in mat4 aModelMatrix;

out vec3 vPosition;
out vec2 vTexCoord;
out mat3 vTBN;
out vec4 vDirectionalShadowPositions[MAX_DIRECTIONAL_SHADOWS];

uniform bool uAnimated;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 uBoneMatrices[MAX_BONES];
uniform mat4 uTransform;
uniform vec4 uClipPlane;
uniform mat4 uDirectionalShadowViewProjectionMatrices[MAX_DIRECTIONAL_SHADOWS];

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

    mat3 normalMatrix = transpose(inverse(mat3(aModelMatrix * transform)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);

    for (int i = 0; i < MAX_DIRECTIONAL_SHADOWS; i++) {
        vDirectionalShadowPositions[i] = uDirectionalShadowViewProjectionMatrices[i] * vec4(vPosition, 1);
    }

    gl_ClipDistance[0] = dot(vec4(vPosition, 1), uClipPlane);
}
)";

const std::string Model::kFsSource = R"(
#version 420 core

const float zero = 1e-6;

uniform vec3 uCameraPosition;

in vec3 vPosition;
in vec2 vTexCoord;
in mat3 vTBN;
)" + LightSources::FsSource() + ShadowSources::FsSource() +
                                     R"(
in vec4 vDirectionalShadowPositions[MAX_DIRECTIONAL_SHADOWS];

uniform bool uDefaultShading;

struct Material {
    bool ambientTextureEnabled;
    sampler2D ambientTexture;
    bool diffuseTextureEnabled;
    sampler2D diffuseTexture;
    bool specularTextureEnabled;
    sampler2D specularTexture;
    bool normalsTextureEnabled;
    sampler2D normalsTexture;
    bool metalnessTextureEnabled;
    sampler2D metalnessTexture;
    bool diffuseRoughnessTextureEnabled;
    sampler2D diffuseRoughnessTexture;
    bool ambientOcclusionTextureEnabled;
    sampler2D ambientOcclusionTexture;
    vec3 ka, kd, ks;
    float shininess;
    bool bindMetalnessAndDiffuseRoughness;
};

uniform Material uMaterial;

vec3 CalcDefaultShading() {
    // gold material
    vec3 color = vec3(0.944, 0.776, 0.373);
    vec3 albedo = pow(color, vec3(2.2));
    float metallic = 0.95;
    float roughness = 0.05;
    return CalcPBRLighting(
        albedo, metallic, roughness, 1,
        vTBN[2], uCameraPosition, vPosition,
        0
    );
}

vec4 CalcFragColorWithPhong() {
    float alpha = 1.0f;

    vec3 ka = vec3(uMaterial.ka);
    vec3 kd = vec3(uMaterial.kd);
    vec3 ks = vec3(uMaterial.ks);

    if (uMaterial.diffuseTextureEnabled) {
        vec4 sampled = texture(uMaterial.diffuseTexture, vTexCoord);
        kd = sampled.rgb;
        alpha = sampled.a;
    }

    if (uMaterial.ambientTextureEnabled) {
        ka = texture(uMaterial.ambientTexture, vTexCoord).rgb;
    }
    if (uMaterial.specularTextureEnabled) {
        ks = texture(uMaterial.specularTexture, vTexCoord).rgb;
    }

    vec3 normal = vTBN[2];
    if (uMaterial.normalsTextureEnabled) {
        normal = texture(uMaterial.normalsTexture, vTexCoord).xyz;
        normal = normalize(vTBN * (normal * 2 - 1));
    }

    float shadow = CalcShadow(vDirectionalShadowPositions, normal);

    vec3 color = CalcPhongLighting(
        ka, kd, ks,
        normal, uCameraPosition, vPosition,
        uMaterial.shininess, shadow
    );

    return vec4(color, alpha);
}

vec4 CalcFragColorWithPBR() {
    float alpha = 1.0f;

    vec3 albedo = vec3(0.0f);
    float metallic = 0.0f;
    float roughness = 0.0f;
    float ao = 1.0f;

    if (uMaterial.diffuseTextureEnabled) {
        vec4 sampled = texture(uMaterial.diffuseTexture, vTexCoord);
        // convert from sRGB to linear space
        albedo = pow(sampled.rgb, vec3(2.2));
        alpha = sampled.a;
    }

    if (uMaterial.bindMetalnessAndDiffuseRoughness) {
        vec2 sampled = texture(uMaterial.metalnessTexture, vTexCoord).gb;
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
        metallic = sampled[1]; // blue
        roughness = sampled[0]; // green
    } else {
        if (uMaterial.metalnessTextureEnabled) {
            metallic = texture(uMaterial.metalnessTexture, vTexCoord).r;
        }
        if (uMaterial.diffuseRoughnessTextureEnabled) {
            roughness = texture(uMaterial.diffuseRoughnessTexture, vTexCoord).r;
        }
    }

    if (uMaterial.ambientOcclusionTextureEnabled) {
        ao = texture(uMaterial.ambientOcclusionTexture, vTexCoord).r;
    }

    vec3 normal = vTBN[2];
    if (uMaterial.normalsTextureEnabled) {
        normal = texture(uMaterial.normalsTexture, vTexCoord).xyz;
        normal = normalize(vTBN * (normal * 2 - 1));
    }

    float shadow = CalcShadow(vDirectionalShadowPositions, normal);

    vec3 color = CalcPBRLighting(
        albedo, metallic, roughness, ao,
        normal, uCameraPosition, vPosition,
        shadow
    );

    return vec4(color, alpha);
}

vec4 CalcFragColor() {
    if (uDefaultShading) {
        return vec4(CalcDefaultShading(), 1);
    }
    if (uMaterial.metalnessTextureEnabled) {
        return CalcFragColorWithPBR();
    } else {
        return CalcFragColorWithPhong();
    }
}
)";

const std::string Model::kFsMainSource = R"(
out vec4 fragColor;

void main() {
    fragColor = CalcFragColor();
}
)";

const std::string Model::kFsOITMainSource = R"(
layout (r32ui) uniform uimage2D uHeadPointers;
layout (binding = 0) uniform atomic_uint uListSize;
layout (rgba32ui) uniform uimageBuffer uList;

void AppendToList(vec4 fragColor) {
    uint index = atomicCounterIncrement(uListSize);
    uint prevHead = imageAtomicExchange(uHeadPointers, ivec2(gl_FragCoord.xy), index);
    uvec4 item;
    item.x = prevHead;
    item.y = packUnorm4x8(fragColor);
    item.z = floatBitsToUint(gl_FragCoord.z);
    imageStore(uList, int(index), item);
}

void main() {
    vec4 fragColor = CalcFragColor();
    AppendToList(fragColor);
}
)";

const std::string Model::kFsShadowMainSource = R"(
void main() {}
)";
