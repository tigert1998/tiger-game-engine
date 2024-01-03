#include "model.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glog/logging.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

#include "utils.h"

namespace fs = std::filesystem;

Model::Model(const std::string &path, MultiDrawIndirect *multi_draw_indirect,
             bool flip_y)
    : directory_path_(fs::path::path(path).parent_path().string()),
      flip_y_(flip_y),
      multi_draw_indirect_(multi_draw_indirect) {
  CompileShaders();

  LOG(INFO) << "loading model at: \"" << path << "\"";

  uint32_t flags = aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
                   aiProcess_Triangulate;
  if (flip_y_) flags |= aiProcess_FlipUVs;
  scene_ = aiImportFile(path.c_str(), flags);

  InitAnimationChannelMap();

  meshes_.resize(scene_->mNumMeshes);
  LOG(INFO) << "#meshes: " << meshes_.size();
  LOG(INFO) << "#animations: " << scene_->mNumAnimations;
  RecursivelyInitNodes(scene_->mRootNode, glm::mat4(1));

  bone_matrices_.resize(bone_namer_.total());

  multi_draw_indirect_->ModelBeginSubmission(this);
  for (int i = 0; i < meshes_.size(); i++) {
    if (meshes_[i] == nullptr) continue;
    meshes_[i]->SubmitToMultiDrawIndirect();
  }
  multi_draw_indirect_->ModelEndSubmission(this, bone_matrices_.size());
}

Model::~Model() { aiReleaseImport(scene_); }

void Model::RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform) {
  auto node_transform = Mat4FromAimatrix4x4(node->mTransformation);
  auto transform = parent_transform * node_transform;

  LOG(INFO) << "initializing node \"" << node->mName.C_Str() << "\"";
  for (int i = 0; i < node->mNumMeshes; i++) {
    int id = node->mMeshes[i];
    if (meshes_[id] == nullptr) {
      auto mesh = scene_->mMeshes[id];
      try {
        meshes_[id].reset(new Mesh(directory_path_, mesh, scene_, &bone_namer_,
                                   &bone_offsets_, flip_y_,
                                   multi_draw_indirect_));
      } catch (std::exception &e) {
        meshes_[id] = nullptr;
        LOG(WARNING) << "not loading mesh \"" << mesh->mName.C_Str()
                     << "\" because an exception is thrown: " << e.what();
      }
    }
    if (meshes_[id] != nullptr) {
      meshes_[id]->AppendTransform(transform);
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
    std::map<std::string, std::any> constants = {
        {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
        {"IS_SHADOW_PASS", std::any(false)},
    };
    kShader.reset(new Shader(
        Model::kVsSource, Model::kFsSource + Model::kFsMainSource, constants));
    kOITShader.reset(new Shader(Model::kVsSource,
                                Model::kFsSource + Model::kFsOITMainSource,
                                constants));
    kDeferredShadingShader.reset(new Shader(
        Model::kVsSource,
        Model::kFsSource + Model::kFsDeferredShadingMainSource, constants));

    constants["IS_SHADOW_PASS"] = std::any(true);
    kShadowShader.reset(new Shader(
        {
            {GL_VERTEX_SHADER, Model::kVsSource},
            {GL_GEOMETRY_SHADER, Model::kGsShadowSource},
            {GL_FRAGMENT_SHADER, Model::kFsShadowSource},
        },
        constants));
  }
}

std::unique_ptr<Shader> Model::kShader = nullptr;
std::unique_ptr<Shader> Model::kOITShader = nullptr;
std::unique_ptr<Shader> Model::kShadowShader = nullptr;
std::unique_ptr<Shader> Model::kDeferredShadingShader = nullptr;

const std::string Model::kVsSource = R"(
#version 460 core

const bool IS_SHADOW_PASS = <!--IS_SHADOW_PASS-->;

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

out vec3 vPosition;
out vec2 vTexCoord;
out mat3 vTBN;
flat out int vInstanceID;

layout (std430, binding = 0) buffer modelMatricesBuffer {
    mat4 modelMatrices[]; // per instance
};
layout (std430, binding = 1) buffer boneMatricesBuffer {
    mat4 boneMatrices[];
};
layout (std430, binding = 2) buffer boneMatricesOffsetBuffer {
    int boneMatricesOffset[]; // per instance
};
layout (std430, binding = 3) buffer animatedBuffer {
    bool animated[]; // per instance
};
layout (std430, binding = 4) buffer transformsBuffer {
    mat4 transforms[]; // per instance
};
layout (std430, binding = 5) buffer clipPlanesBuffer {
    vec4 clipPlanes[]; // per instance
};

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

mat4 CalcBoneMatrix() {
    int offset = boneMatricesOffset[vInstanceID];
    mat4 boneMatrix = mat4(0);
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs0[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs0[i] + offset] * aBoneWeights0[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs1[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs1[i] + offset] * aBoneWeights1[i];
    }
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs2[i] < 0) return boneMatrix;
        boneMatrix += boneMatrices[aBoneIDs2[i] + offset] * aBoneWeights2[i];
    }
    return boneMatrix;
}

void main() {
    vInstanceID = gl_BaseInstance + gl_InstanceID;
    mat4 transform;
    if (animated[vInstanceID]) {
        transform = CalcBoneMatrix();
    } else {
        transform = transforms[vInstanceID];
    }
    mat4 modelMatrix = modelMatrices[vInstanceID];
    if (IS_SHADOW_PASS) {
        gl_Position = modelMatrix * transform * vec4(aPosition, 1);
    } else {
        gl_Position = uProjectionMatrix * uViewMatrix * modelMatrix * transform * vec4(aPosition, 1);
        vPosition = vec3(modelMatrix * transform * vec4(aPosition, 1));
        vTexCoord = aTexCoord;

        mat3 normalMatrix = transpose(inverse(mat3(modelMatrix * transform)));
        vec3 T = normalize(normalMatrix * aTangent);
        vec3 N = normalize(normalMatrix * aNormal);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        vTBN = mat3(T, B, N);

        gl_ClipDistance[0] = dot(vec4(vPosition, 1), clipPlanes[vInstanceID]);
    }
}
)";

const std::string Model::kFsSource = R"(
#version 460 core

#extension GL_ARB_bindless_texture : require

const float zero = 1e-6;

uniform vec3 uCameraPosition;
uniform mat4 uViewMatrix;

in vec3 vPosition;
in vec2 vTexCoord;
in mat3 vTBN;
flat in int vInstanceID;
)" + LightSources::FsSource() + ShadowSources::FsSource() +
                                     R"(
uniform bool uDefaultShading;

struct Material {
    int ambientTexture;
    int diffuseTexture;
    int specularTexture;
    int normalsTexture;
    int metalnessTexture;
    int diffuseRoughnessTexture;
    int ambientOcclusionTexture;
    vec4 ka, kd, ks;
    float shininess;
    bool bindMetalnessAndDiffuseRoughness;
};

layout (std430, binding = 6) buffer materialsBuffer {
    Material materials[]; // per instance
};
layout (std430, binding = 7) buffer texturesBuffer {
    sampler2D textures[];
};

vec3 ConvertDerivativeMapToNormalMap(vec3 normal) {
    if (normal.z > -1 + zero) return normal;
    return normalize(vec3(-normal.x, -normal.y, 1));
}

void SampleForGBuffer(
    out vec3 ka, out vec3 kd, out vec3 ks, out float shininess, // for Phong
    out vec3 albedo, out float metallic, out float roughness, out float ao, // for PBR
    out vec3 normal, out vec3 position, out float alpha, out int flag // shared variable
) {
    if (uDefaultShading) {
        albedo = pow(vec3(0.944, 0.776, 0.373), vec3(2.2));
        metallic = 0.95;
        roughness = 0.05;
        ao = 1;
        normal = vTBN[2];
        position = vPosition;
        alpha = 1;
        flag = 2;
        return;
    }

    Material material = materials[vInstanceID];

    if (material.metalnessTexture < 0) {
        // for Phong
        alpha = 1.0f;

        ka = vec3(material.ka);
        kd = vec3(material.kd);
        ks = vec3(material.ks);

        if (material.diffuseTexture >= 0) {
            vec4 sampled = texture(textures[material.diffuseTexture], vTexCoord);
            kd = sampled.rgb;
            alpha = sampled.a;
        }

        if (alpha <= zero) discard;

        if (material.ambientTexture >= 0) {
            ka = texture(textures[material.ambientTexture], vTexCoord).rgb;
        }
        if (material.specularTexture >= 0) {
            ks = texture(textures[material.specularTexture], vTexCoord).rgb;
        }

        shininess = material.shininess;
        flag = 1;
    } else {
        // for PBR
        alpha = 1.0f;

        albedo = vec3(0.0f);
        metallic = 0.0f;
        roughness = 0.0f;
        ao = 1.0f;

        if (material.diffuseTexture >= 0) {
            vec4 sampled = texture(textures[material.diffuseTexture], vTexCoord);
            // convert from sRGB to linear space
            albedo = pow(sampled.rgb, vec3(2.2));
            alpha = sampled.a;
        }

        if (alpha <= zero) discard;

        if (material.bindMetalnessAndDiffuseRoughness) {
            vec2 sampled = texture(textures[material.metalnessTexture], vTexCoord).gb;
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
            metallic = sampled[1]; // blue
            roughness = sampled[0]; // green
        } else {
            if (material.metalnessTexture >= 0) {
                metallic = texture(textures[material.metalnessTexture], vTexCoord).r;
            }
            if (material.diffuseRoughnessTexture >= 0) {
                roughness = texture(textures[material.diffuseRoughnessTexture], vTexCoord).r;
            }
        }

        if (material.ambientOcclusionTexture >= 0) {
            ao = texture(textures[material.ambientOcclusionTexture], vTexCoord).r;
        }
        flag = 2;
    }

    normal = vTBN[2];
    if (material.normalsTexture >= 0) {
        normal = texture(textures[material.normalsTexture], vTexCoord).xyz * 2 - 1;
        normal = ConvertDerivativeMapToNormalMap(normal);
        normal = normalize(vTBN * normal);
    }

    position = vPosition;
}

vec4 CalcFragColor() {
    vec3 ka; vec3 kd; vec3 ks; float shininess; // for Phong
    vec3 albedo; float metallic; float roughness; float ao; // for PBR
    vec3 normal; vec3 position; float alpha; int flag; // shared variable

    SampleForGBuffer(
        ka, kd, ks, shininess, // for Phong
        albedo, metallic, roughness, ao, // for PBR
        normal, position, alpha, flag // shared variable
    );

    float shadow = CalcShadow(position, normal);
    vec3 color;

    if (flag == 1) {
        color = CalcPhongLighting(
            ka, kd, ks,
            normal, uCameraPosition, position,
            shininess, shadow
        );
    } else if (flag == 2) {
        color = CalcPBRLighting(
            albedo, metallic, roughness, ao,
            normal, uCameraPosition, position,
            shadow
        );
    }

    return vec4(color, alpha);
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

const std::string Model::kGsShadowSource = R"(
#version 460 core

layout (triangles, invocations = <!--NUM_CASCADES-->) in;
layout (triangle_strip, max_vertices = 3) out;

uniform mat4 uDirectionalShadowViewProjectionMatrices[<!--NUM_CASCADES-->];

void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = uDirectionalShadowViewProjectionMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
)";

const std::string Model::kFsShadowSource = R"(
void main() {}
)";

const std::string Model::kFsDeferredShadingMainSource = R"(
layout (location = 0) out vec3 ka;
layout (location = 1) out vec3 kd;
layout (location = 2) out vec4 ksAndShininess; // for Phong
layout (location = 3) out vec3 albedo;
layout (location = 4) out vec3 metallicAndRoughnessAndAo; // for PBR
layout (location = 5) out vec3 normal;
layout (location = 6) out vec4 positionAndAlpha;
layout (location = 7) out int flag; // shared variable

void main() {
    SampleForGBuffer(
        ka, kd, ksAndShininess.xyz, ksAndShininess.w,
        // for Phong
        albedo, metallicAndRoughnessAndAo.x, metallicAndRoughnessAndAo.y, metallicAndRoughnessAndAo.z,
        // for PBR
        normal, positionAndAlpha.xyz, positionAndAlpha.w, flag
        // shared variable
    );
    if (positionAndAlpha.w < 0.5) {
        // punch-through method
        discard;
    }
}
)";