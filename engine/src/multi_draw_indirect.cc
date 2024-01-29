#include "multi_draw_indirect.h"

#include <fmt/core.h>
#include <glad/glad.h>

#include <iterator>
#include <set>

#include "model.h"
#include "utils.h"

GPUDrivenWorkloadGeneration::GPUDrivenWorkloadGeneration(
    const FixedArrays &fixed_arrays, const DynamicBuffers &dynamic_buffers,
    const Constants &constants)
    : dynamic_buffers_(dynamic_buffers), constants_(constants) {
  CompileShaders();
  AllocateBuffers(fixed_arrays);
}

void GPUDrivenWorkloadGeneration::CompileShaders() {
  if (frustum_culling_and_lod_selection_shader_ == nullptr &&
      prefix_sum_shader_ == nullptr && remap_shader_ == nullptr) {
    frustum_culling_and_lod_selection_shader_.reset(new Shader(
        {{GL_COMPUTE_SHADER, kCsFrustumCullingAndLodSelectionSource}}, {}));
    prefix_sum_shader_.reset(
        new Shader({{GL_COMPUTE_SHADER, kCsPrefixSumSource}}, {}));
    remap_shader_.reset(new Shader({{GL_COMPUTE_SHADER, kCsRemapSource}}, {}));
  }
}

void GPUDrivenWorkloadGeneration::AllocateBuffers(
    const FixedArrays &fixed_arrays) {
  // constant buffers
  aabbs_ssbo_.reset(new SSBO(*fixed_arrays.aabbs, GL_STATIC_DRAW, 0));
  instance_to_mesh_ssbo_.reset(
      new SSBO(*fixed_arrays.instance_to_mesh, GL_STATIC_DRAW, 1));
  mesh_to_cmd_offset_ssbo_.reset(
      new SSBO(*fixed_arrays.mesh_to_cmd_offset, GL_STATIC_DRAW, 2));
  mesh_to_num_cmds_ssbo_.reset(
      new SSBO(*fixed_arrays.mesh_to_num_cmds, GL_STATIC_DRAW, 3));

  // intermediate buffers
  cmd_instance_count_ssbo_.reset(new SSBO(
      constants_.num_commands * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW, 6));
  instance_to_cmd_ssbo_.reset(new SSBO(
      constants_.num_instances * sizeof(int32_t), nullptr, GL_DYNAMIC_DRAW, 7));
}

void GPUDrivenWorkloadGeneration::Compute() {
  // compute frustum culling and lod selection
  frustum_culling_and_lod_selection_shader_->Use();
  aabbs_ssbo_->BindBufferBase(0);
  dynamic_buffers_.input_model_matrices_ssbo->BindBufferBase(1);
  instance_to_mesh_ssbo_->BindBufferBase(2);
  dynamic_buffers_.frustum_ssbo->BindBufferBase(3);
  mesh_to_cmd_offset_ssbo_->BindBufferBase(4);
  mesh_to_num_cmds_ssbo_->BindBufferBase(5);
  uint32_t zero = 0;
  glClearNamedBufferData(cmd_instance_count_ssbo_->id(), GL_R32UI,
                         GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
  cmd_instance_count_ssbo_->BindBufferBase(6);
  instance_to_cmd_ssbo_->BindBufferBase(7);
  frustum_culling_and_lod_selection_shader_->SetUniform<uint32_t>(
      "uInstanceCount", constants_.num_instances);
  glDispatchCompute((constants_.num_instances + 255) / 256, 1, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // compute prefix sum
  prefix_sum_shader_->Use();
  cmd_instance_count_ssbo_->BindBufferBase(0);
  dynamic_buffers_.commands_ssbo->BindBufferBase(1);
  prefix_sum_shader_->SetUniform<uint32_t>("uCmdCount",
                                           constants_.num_commands);
  glDispatchCompute((constants_.num_commands + 1023) / 1024, 1, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  // compute remap
  remap_shader_->Use();
  instance_to_cmd_ssbo_->BindBufferBase(0);
  dynamic_buffers_.commands_ssbo->BindBufferBase(1);
  uint32_t zero = 0;
  glClearNamedBufferData(cmd_instance_count_ssbo_->id(), GL_R32UI,
                         GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
  cmd_instance_count_ssbo_->BindBufferBase(2);
  dynamic_buffers_.input_model_matrices_ssbo->BindBufferBase(3);
  dynamic_buffers_.input_bone_matrices_offset_ssbo->BindBufferBase(4);
  dynamic_buffers_.input_animated_ssbo->BindBufferBase(5);
  dynamic_buffers_.input_transforms_ssbo->BindBufferBase(6);
  dynamic_buffers_.input_clip_planes_ssbo->BindBufferBase(7);
  dynamic_buffers_.input_materials_ssbo->BindBufferBase(8);
  dynamic_buffers_.model_matrices_ssbo->BindBufferBase(9);
  dynamic_buffers_.bone_matrices_offset_ssbo->BindBufferBase(10);
  dynamic_buffers_.animated_ssbo->BindBufferBase(11);
  dynamic_buffers_.transforms_ssbo->BindBufferBase(12);
  dynamic_buffers_.clip_planes_ssbo->BindBufferBase(13);
  dynamic_buffers_.materials_ssbo->BindBufferBase(14);
  remap_shader_->SetUniform<uint32_t>("uInstanceCount",
                                      constants_.num_instances);
  glDispatchCompute((constants_.num_instances + 255) / 256, 1, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

const std::string
    GPUDrivenWorkloadGeneration::kCsFrustumCullingAndLodSelectionSource =
        std::string(R"(
#version 460 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
)") + AABB::GLSLSource() +
        R"(
layout (std430, binding = 0) readonly buffer aabbsBuffer {
    AABB aabbs[]; // per mesh
};
layout (std430, binding = 1) readonly buffer modelMatricesBuffer {
    mat4 modelMatrices[]; // per instance
};
layout (std430, binding = 2) readonly buffer instanceToMeshBuffer {
    uint instanceToMesh[]; // per instance
};
layout (std430, binding = 3) readonly buffer frustumBuffer {
    Frustum cameraFrustum;
};
layout (std430, binding = 4) readonly buffer meshToCmdOffsetBuffer {
    uint meshToCmdOffset[]; // per mesh
};
layout (std430, binding = 5) readonly buffer meshToNumCmdsBuffer {
    uint meshToNumCmds[]; // per mesh
};
layout (std430, binding = 6) buffer cmdInstanceCountBuffer {
    uint cmdInstanceCount[]; // per cmd
};
layout (std430, binding = 7) writeonly buffer instanceToCmdBuffer {
    int instanceToCmd[]; // per instance
};

uniform uint uInstanceCount;

void main() {
    uint instanceID = gl_GlobalInvocationID.x; 
    if (instanceID >= uInstanceCount) return;

    uint meshID = instanceToMesh[instanceID];
    AABB newAABB = TransformAABB(modelMatrices[instanceID], aabbs[meshID]);
    bool doRender = AABBIsOnFrustum(newAABB, cameraFrustum);

    // put lod selection here
    uint cmdID = meshToCmdOffset[meshID];
    atomicAdd(cmdInstanceCount[cmdID], uint(doRender));
    instanceToCmd[instanceID] = doRender ? cmdID : -1;
}
)";

const std::string GPUDrivenWorkloadGeneration::kCsPrefixSumSource = R"(
#version 460 core

layout (local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint baseInstance;
};

layout (std430, binding = 0) readonly buffer cmdInstanceCountBuffer {
    uint cmdInstanceCount[]; // per cmd
};
layout (std430, binding = 1) buffer commandsBuffer {
    DrawElementsIndirectCommand commands[]; // per cmd
};

uniform uint uCmdCount;

void main() {
    uint cmdID = gl_GlobalInvocationID.x;
    if (cmdID >= uCmdCount) return;

    commands[cmdID].baseInstance = cmdInstanceCount[cmdID];
    barrier();

    uint step = 0;
    while (uCmdCount > (1 << step)) {
        if ((cmdID & (1 << step)) > 0) {
            uint readCmdID = (cmdID | ((1 << step) - 1)) - (1 << step);
            commands[cmdID].baseInstance += commands[readCmdID].baseInstance;
        }
        barrier();
        step += 1;
    }

    commands[cmdID].baseInstance -= cmdInstanceCount[cmdID];
    commands[cmdID].instanceCount = cmdInstanceCount[cmdID];
}
)";

const std::string GPUDrivenWorkloadGeneration::kCsRemapSource = R"(
#version 460 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint baseInstance;
};

layout (std430, binding = 0) readonly buffer instanceToCmdBuffer {
    int instanceToCmd[]; // per instance
};
layout (std430, binding = 1) readonly buffer commandsBuffer {
    DrawElementsIndirectCommand commands[]; // per cmd
};
layout (std430, binding = 2) buffer cmdInstanceCountBuffer {
    uint cmdInstanceCount[] // per cmd
};

// input
layout (std430, binding = 3) readonly buffer inputModelMatricesBuffer {
    mat4 inputModelMatrices[]; // per instance
};
layout (std430, binding = 4) readonly buffer inputBoneMatricesOffsetBuffer {
    int inputBoneMatricesOffset[]; // per instance
};
layout (std430, binding = 5) readonly buffer inputAnimatedBuffer {
    uint inputAnimated[]; // per instance
};
layout (std430, binding = 6) readonly buffer inputTransformsBuffer {
    mat4 inputTransforms[]; // per instance
};
layout (std430, binding = 7) readonly buffer inputClipPlanesBuffer {
    vec4 inputClipPlanes[]; // per instance
};
layout (std430, binding = 8) readonly buffer inputMaterialsBuffer {
    Material inputMaterials[]; // per instance
};

// output
layout (std430, binding = 9) writeonly buffer modelMatricesBuffer {
    mat4 modelMatrices[]; // per instance
};
layout (std430, binding = 10) writeonly buffer boneMatricesOffsetBuffer {
    int boneMatricesOffset[]; // per instance
};
layout (std430, binding = 11) writeonly buffer animatedBuffer {
    uint animated[]; // per instance
};
layout (std430, binding = 12) writeonly buffer transformsBuffer {
    mat4 transforms[]; // per instance
};
layout (std430, binding = 13) writeonly buffer clipPlanesBuffer {
    vec4 clipPlanes[]; // per instance
};
layout (std430, binding = 14) writeonly buffer materialsBuffer {
    Material materials[]; // per instance
};

uniform uint uInstanceCount;

void main() {
    uint instanceID = gl_GlobalInvocationID.x;
    if (instanceID >= uInstanceCount || instanceToCmd[instanceID] < 0) return;
    uint cmdID = uint(instanceToCmd[instanceID]);
    uint newInstanceID = atomicAdd(cmdInstanceCount[cmdID], 1) + commands[cmdID].baseInstance;

    modelMatrices[newInstanceID] = inputModelMatrices[instanceID];
    boneMatricesOffset[newInstanceID] = inputBoneMatricesOffset[instanceID];
    animated[newInstanceID] = inputAnimated[instanceID];
    transforms[newInstanceID] = inputTransforms[instanceID];
    clipPlanes[newInstanceID] = inputClipPlanes[instanceID];
    materials[newInstanceID] = inputMaterials[instanceID];
}
)";

MultiDrawIndirect::~MultiDrawIndirect() {
  glDeleteBuffers(1, &commands_buffer_);
  glDeleteBuffers(1, &vao_);
  glDeleteBuffers(1, &ebo_);
  glDeleteBuffers(1, &vbo_);
}

void MultiDrawIndirect::CheckRenderTargetParameter(
    const std::vector<RenderTargetParameter> &render_target_params) {
  const std::string all_models_must_present_error_message =
      "all models must be present in the parameters";
  for (const auto &param : render_target_params) {
    if (model_to_item_count_.count(param.model) == 0) {
      fmt::print(stderr, "[error] {}\n", all_models_must_present_error_message);
      exit(1);
    }
    if (model_to_item_count_[param.model] != param.items.size()) {
      fmt::print(stderr, "[error] model item count mismatch\n");
      exit(1);
    }
  }
  if (model_to_item_count_.size() != render_target_params.size()) {
    fmt::print(stderr, "[error] {}\n", all_models_must_present_error_message);
    exit(1);
  }
}

void MultiDrawIndirect::UpdateBuffers(
    const std::vector<RenderTargetParameter> &render_target_params) {
  for (const auto &param : render_target_params) {
    for (int item_idx = 0; item_idx < param.items.size(); item_idx++) {
      const auto &item = param.items[item_idx];
      bool item_is_animated = 0 <= item.animation_id &&
                              item.animation_id < param.model->NumAnimations();

      if (item_is_animated) {
        param.model->RecursivelyUpdateBoneMatrices(
            item.animation_id, param.model->scene_->mRootNode, glm::mat4(1),
            item.time * param.model->scene_->mAnimations[item.animation_id]
                            ->mTicksPerSecond);
        uint32_t offset =
            item_to_bone_matrices_offset_[{param.model, item_idx}];
        std::copy(param.model->bone_matrices_.begin(),
                  param.model->bone_matrices_.end(),
                  bone_matrices_.begin() + offset);
      }

      uint32_t instance_offset =
          item_to_instance_offset_[{param.model, item_idx}];
      uint32_t item_count = param.items.size();
      for (int i = 0, j = 0; i < param.model->meshes_.size(); i++) {
        if (param.model->meshes_[i] == nullptr) continue;
        // animated
        animated_[instance_offset + j * item_count] =
            has_bone_[instance_offset + j * item_count] && item_is_animated;
        model_matrices_[instance_offset + j * item_count] = item.model_matrix;
        clip_planes_[instance_offset + j * item_count] = item.clip_plane;
        j++;
      }
    }
  }

  glNamedBufferSubData(bone_matrices_ssbo_->id(), 0,
                       bone_matrices_.size() * sizeof(bone_matrices_[0]),
                       bone_matrices_.data());
  glNamedBufferSubData(input_animated_ssbo_->id(), 0,
                       animated_.size() * sizeof(animated_[0]),
                       animated_.data());
  glNamedBufferSubData(input_model_matrices_ssbo_->id(), 0,
                       model_matrices_.size() * sizeof(model_matrices_[0]),
                       model_matrices_.data());
  glNamedBufferSubData(input_clip_planes_ssbo_->id(), 0,
                       clip_planes_.size() * sizeof(clip_planes_[0]),
                       clip_planes_.data());
}

void MultiDrawIndirect::BindBuffers() {
  glBindVertexArray(vao_);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands_buffer_);

  model_matrices_ssbo_->BindBufferBase(0);
  bone_matrices_ssbo_->BindBufferBase(1);
  bone_matrices_offset_ssbo_->BindBufferBase(2);
  animated_ssbo_->BindBufferBase(3);
  transforms_ssbo_->BindBufferBase(4);
  clip_planes_ssbo_->BindBufferBase(5);
  materials_ssbo_->BindBufferBase(6);
  textures_ssbo_->BindBufferBase(7);
}

void MultiDrawIndirect::DrawDepthForShadow(
    Shadow *shadow,
    const std::vector<RenderTargetParameter> &render_target_params) {
  CheckRenderTargetParameter(render_target_params);
  UpdateBuffers(render_target_params);

  BindBuffers();
  Model::kShadowShader->Use();
  shadow->SetForDepthPass(Model::kShadowShader.get());

  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                              commands_.size(), 0);
  glBindVertexArray(0);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MultiDrawIndirect::Draw(
    Camera *camera, LightSources *light_sources, ShadowSources *shadow_sources,
    OITRenderQuad *oit_render_quad, bool deferred_shading, bool default_shading,
    bool force_pbr,
    const std::vector<RenderTargetParameter> &render_target_params) {
  CheckRenderTargetParameter(render_target_params);
  UpdateBuffers(render_target_params);

  glNamedBufferSubData(frustum_ssbo_->id(), 0, sizeof(Frustum),
                       &camera->frustum());
  gpu_driven_->Compute();

  Shader *shader = nullptr;
  if (oit_render_quad != nullptr) {
    shader = Model::kOITShader.get();
  } else if (deferred_shading) {
    shader = Model::kDeferredShadingShader.get();
  } else {
    shader = Model::kShader.get();
  }

  BindBuffers();
  shader->Use();
  if (oit_render_quad != nullptr) {
    oit_render_quad->Set(shader);
  }
  if (!deferred_shading) {
    light_sources->Set(shader, false);
    int32_t num_samplers = 0;
    shadow_sources->Set(shader, &num_samplers);
    shader->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  }
  shader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  shader->SetUniform<glm::mat4>("uProjectionMatrix",
                                camera->projection_matrix());
  shader->SetUniform<int32_t>("uDefaultShading", default_shading);
  shader->SetUniform<int32_t>("uForcePBR", force_pbr);

  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                              commands_.size(), 0);
  glBindVertexArray(0);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void MultiDrawIndirect::PrepareForDraw() {
  // commands buffer
  glCreateBuffers(1, &commands_buffer_);
  glNamedBufferStorage(commands_buffer_,
                       sizeof(DrawElementsIndirectCommand) * commands_.size(),
                       (const void *)commands_.data(), GL_DYNAMIC_STORAGE_BIT);

  // vao
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &ebo_);
  glGenBuffers(1, &vbo_);

  glBindVertexArray(vao_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VertexWithBones) * vertices_.size(),
               vertices_.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices_.size(),
               indices_.data(), GL_STATIC_DRAW);

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

  // create SSBO
  input_model_matrices_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::mat4),
                                            nullptr, GL_DYNAMIC_DRAW, 0));
  bone_matrices_ssbo_.reset(new SSBO(num_bone_matrices_ * sizeof(glm::mat4),
                                     nullptr, GL_DYNAMIC_DRAW, 0));
  input_bone_matrices_offset_ssbo_.reset(
      new SSBO(bone_matrices_offset_, GL_STATIC_DRAW, 0));
  input_animated_ssbo_.reset(new SSBO(has_bone_.size() * sizeof(has_bone_[0]),
                                      nullptr, GL_DYNAMIC_DRAW, 0));
  input_transforms_ssbo_.reset(new SSBO(transforms_, GL_STATIC_DRAW, 0));
  input_clip_planes_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::vec4),
                                         nullptr, GL_DYNAMIC_DRAW, 0));
  input_materials_ssbo_.reset(new SSBO(materials_, GL_STATIC_DRAW, 0));
  textures_ssbo_.reset(new SSBO(texture_handles_, GL_STATIC_DRAW, 0));

  model_matrices_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::mat4),
                                      nullptr, GL_DYNAMIC_DRAW, 0));
  bone_matrices_offset_ssbo_.reset(
      new SSBO(bone_matrices_offset_, GL_DYNAMIC_DRAW, 0));
  animated_ssbo_.reset(new SSBO(has_bone_.size() * sizeof(has_bone_[0]),
                                nullptr, GL_DYNAMIC_DRAW, 0));
  transforms_ssbo_.reset(new SSBO(transforms_, GL_DYNAMIC_DRAW, 0));
  clip_planes_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::vec4), nullptr,
                                   GL_DYNAMIC_DRAW, 0));
  materials_ssbo_.reset(new SSBO(materials_, GL_DYNAMIC_DRAW, 0));

  commands_ssbo_.reset(new SSBO(commands_buffer_, 0, false));
  frustum_ssbo_.reset(new SSBO(sizeof(Frustum), nullptr, GL_DYNAMIC_DRAW, 0));

  bone_matrices_.resize(num_bone_matrices_);
  animated_.resize(num_instances_);
  model_matrices_.resize(num_instances_);
  clip_planes_.resize(num_instances_);
  std::set<uint32_t> ids;
  for (int i = 0; i < textures_.size(); i++) {
    if (ids.count(textures_[i].id()) == 0) {
      textures_[i].MakeResident();
    }
    ids.insert(textures_[i].id());
  }
  CHECK_OPENGL_ERROR();

  GPUDrivenWorkloadGeneration::FixedArrays fixed_arrays;
  fixed_arrays.aabbs = &aabbs_;
  fixed_arrays.instance_to_mesh = &instance_to_mesh_;
  fixed_arrays.mesh_to_cmd_offset = &mesh_to_cmd_offset_;
  fixed_arrays.mesh_to_num_cmds = &mesh_to_num_cmds_;

  GPUDrivenWorkloadGeneration::DynamicBuffers dynamic_buffers;
  dynamic_buffers.input_model_matrices_ssbo = input_model_matrices_ssbo_.get();
  dynamic_buffers.frustum_ssbo = frustum_ssbo_.get();
  dynamic_buffers.commands_ssbo = commands_ssbo_.get();
  dynamic_buffers.input_bone_matrices_offset_ssbo =
      input_bone_matrices_offset_ssbo_.get();
  dynamic_buffers.input_animated_ssbo = input_animated_ssbo_.get();
  dynamic_buffers.input_transforms_ssbo = input_transforms_ssbo_.get();
  dynamic_buffers.input_clip_planes_ssbo = input_clip_planes_ssbo_.get();
  dynamic_buffers.input_materials_ssbo = input_materials_ssbo_.get();
  dynamic_buffers.model_matrices_ssbo = model_matrices_ssbo_.get();
  dynamic_buffers.bone_matrices_offset_ssbo = bone_matrices_offset_ssbo_.get();
  dynamic_buffers.animated_ssbo = animated_ssbo_.get();
  dynamic_buffers.transforms_ssbo = transforms_ssbo_.get();
  dynamic_buffers.clip_planes_ssbo = clip_planes_ssbo_.get();
  dynamic_buffers.materials_ssbo = materials_ssbo_.get();

  GPUDrivenWorkloadGeneration::Constants constants;
  constants.num_commands = commands_.size();
  constants.num_instances = num_instances_;
  gpu_driven_.reset(new GPUDrivenWorkloadGeneration(
      fixed_arrays, dynamic_buffers, constants));
}

void MultiDrawIndirect::Receive(
    const std::vector<VertexWithBones> &vertices,
    const std::vector<std::vector<uint32_t>> &indices,
    const std::vector<TextureRecord> &texture_records,
    const PhongMaterial &phong_material, bool has_bone, glm::mat4 transform,
    AABB aabb) {
  aabbs_.push_back(aabb);
  for (int i = 0; i < submission_cache_.item_count; i++) {
    instance_to_mesh_.push_back(num_meshes_);
  }
  mesh_to_cmd_offset_.push_back(commands_.size());
  mesh_to_num_cmds_.push_back(indices.size());

  for (int i = 0; i < indices.size(); i++) {
    DrawElementsIndirectCommand cmd;
    cmd.count = indices[i].size();
    cmd.instance_count = 0;
    cmd.first_index = indices_.size();
    cmd.base_vertex = vertices_.size();
    cmd.base_instance = 0;
    commands_.push_back(cmd);

    std::copy(indices[i].begin(), indices[i].end(),
              std::back_inserter(indices_));
  }

  std::copy(vertices.begin(), vertices.end(), std::back_inserter(vertices_));

  Material material;
  for (int i = 0; i < texture_records.size(); i++) {
    if (!texture_records[i].enabled) {
      material.textures[i] = -1;
    } else {
      textures_.push_back(texture_records[i].texture.Reference());
      texture_handles_.push_back(textures_.back().handle());
      material.textures[i] = textures_.size() - 1;
    }
  }
  material.ka = phong_material.ka;
  material.kd = phong_material.kd;
  material.ks = phong_material.ks;
  material.shininess = phong_material.shininess;
  if (texture_records[4].type != "METALNESS" ||
      texture_records[5].type != "DIFFUSE_ROUGHNESS") {
    fmt::print(
        stderr,
        "[error] wrong metalness and diffuse roughness texture location\n");
    exit(1);
  }
  material.bind_metalness_and_diffuse_roughness =
      texture_records[4].enabled && texture_records[5].enabled &&
      texture_records[4].texture.id() == texture_records[5].texture.id();

  for (int i = 0; i < submission_cache_.item_count; i++) {
    materials_.push_back(material);
    bone_matrices_offset_.push_back(num_bone_matrices_ +
                                    submission_cache_.num_bone_matrices * i);
    has_bone_.push_back(has_bone);
    transforms_.push_back(transform);
  }

  num_instances_ += submission_cache_.item_count;
  num_meshes_ += 1;
}