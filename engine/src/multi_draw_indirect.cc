#include "multi_draw_indirect.h"

#include <fmt/core.h>
#include <glad/glad.h>

#include <iterator>
#include <set>

#include "model.h"
#include "utils.h"

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
  glNamedBufferSubData(animated_ssbo_->id(), 0,
                       animated_.size() * sizeof(animated_[0]),
                       animated_.data());
  glNamedBufferSubData(model_matrices_ssbo_->id(), 0,
                       model_matrices_.size() * sizeof(model_matrices_[0]),
                       model_matrices_.data());
  glNamedBufferSubData(clip_planes_ssbo_->id(), 0,
                       clip_planes_.size() * sizeof(clip_planes_[0]),
                       clip_planes_.data());
}

void MultiDrawIndirect::BindBuffers() {
  glBindVertexArray(vao_);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commands_buffer_);

  model_matrices_ssbo_->BindBufferBase();
  bone_matrices_ssbo_->BindBufferBase();
  bone_matrices_offset_ssbo_->BindBufferBase();
  animated_ssbo_->BindBufferBase();
  transforms_ssbo_->BindBufferBase();
  clip_planes_ssbo_->BindBufferBase();
  materials_ssbo_->BindBufferBase();
  textures_ssbo_->BindBufferBase();
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
  model_matrices_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::mat4),
                                      nullptr, GL_DYNAMIC_DRAW, 0));
  bone_matrices_ssbo_.reset(new SSBO(num_bone_matrices_ * sizeof(glm::mat4),
                                     nullptr, GL_DYNAMIC_DRAW, 1));
  bone_matrices_offset_ssbo_.reset(
      new SSBO(bone_matrices_offset_.size() * sizeof(bone_matrices_offset_[0]),
               bone_matrices_offset_.data(), GL_STATIC_DRAW, 2));
  animated_ssbo_.reset(new SSBO(has_bone_.size() * sizeof(has_bone_[0]),
                                nullptr, GL_DYNAMIC_DRAW, 3));
  transforms_ssbo_.reset(new SSBO(transforms_.size() * sizeof(transforms_[0]),
                                  transforms_.data(), GL_STATIC_DRAW, 4));
  clip_planes_ssbo_.reset(new SSBO(num_instances_ * sizeof(glm::vec4), nullptr,
                                   GL_DYNAMIC_DRAW, 5));
  materials_ssbo_.reset(new SSBO(materials_.size() * sizeof(materials_[0]),
                                 materials_.data(), GL_STATIC_DRAW, 6));
  textures_ssbo_.reset(
      new SSBO(texture_handles_.size() * sizeof(texture_handles_[0]),
               texture_handles_.data(), GL_STATIC_DRAW, 7));

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
}

void MultiDrawIndirect::Receive(
    const std::vector<VertexWithBones> &vertices,
    const std::vector<uint32_t> &indices,
    const std::vector<TextureRecord> &texture_records,
    const PhongMaterial &phong_material, bool has_bone, glm::mat4 transform) {
  DrawElementsIndirectCommand cmd;
  cmd.count = indices.size();
  cmd.instance_count = submission_cache_.item_count;
  cmd.first_index = indices_.size();
  cmd.base_vertex = vertices_.size();
  cmd.base_instance = num_instances_;

  std::copy(vertices.begin(), vertices.end(), std::back_inserter(vertices_));
  std::copy(indices.begin(), indices.end(), std::back_inserter(indices_));
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

  commands_.push_back(cmd);
}