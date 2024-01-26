#ifndef MULTI_DRAW_INDIRECT_H_
#define MULTI_DRAW_INDIRECT_H_

#include <stdint.h>

#include <glm/glm.hpp>
#include <map>
#include <vector>

#include "camera.h"
#include "light_sources.h"
#include "oit_render_quad.h"
#include "shader.h"
#include "shadow_sources.h"
#include "ssbo.h"
#include "texture.h"
#include "vertex.h"

constexpr int kMaxBonesPerVertex = 12;

using VertexWithBones = Vertex<kMaxBonesPerVertex>;

struct TextureRecord {
  std::string type;  // texture type, e.g. AMBIENT
  bool enabled;      // whether the model contains this kind of texture
  Texture texture;   // OpenGL texture ID
  int32_t op;        // ASSIMP aiTextureOp, not used yet in the shader
  float blend;
  glm::vec3 base_color;  // the base color for this kind of texture, not used
                         // yet in the shader

  inline explicit TextureRecord() = default;

  inline explicit TextureRecord(std::string type, bool enabled, Texture texture,
                                int32_t op, float blend, glm::vec3 base_color)
      : type(type),
        enabled(enabled),
        texture(texture),
        op(op),
        blend(blend),
        base_color(base_color) {}

  template <typename T>
  inline TextureRecord(T &&record)
      : type(record.type),
        enabled(record.enabled),
        texture(record.texture),
        op(record.op),
        blend(record.blend),
        base_color(record.base_color) {}
};

struct PhongMaterial {
  glm::vec3 ka, kd, ks;
  float shininess;
};

struct DrawElementsIndirectCommand {
  uint32_t count;
  uint32_t instance_count;
  uint32_t first_index;
  int32_t base_vertex;
  uint32_t base_instance;
};

class Model;

class MultiDrawIndirect {
 public:
  struct RenderTargetParameter {
    Model *model;
    struct ItemParameter {
      int32_t animation_id;
      double time;
      glm::mat4 model_matrix;
      glm::vec4 clip_plane;
    };
    std::vector<ItemParameter> items;
  };

  void Receive(const std::vector<VertexWithBones> &vertices,
               const std::vector<uint32_t> &indices,
               const std::vector<TextureRecord> &textures,
               const PhongMaterial &phong_material, bool has_bone,
               glm::mat4 transform);

  inline void ModelBeginSubmission(Model *model, uint32_t item_count,
                                   uint32_t num_bone_matrices) {
    submission_cache_.model = model;
    submission_cache_.item_count = item_count;
    submission_cache_.num_bone_matrices = num_bone_matrices;

    for (int i = 0; i < submission_cache_.item_count; i++) {
      item_to_instance_offset_[{submission_cache_.model, i}] =
          num_instances_ + i;
    }
    model_to_item_count_[submission_cache_.model] =
        submission_cache_.item_count;
  }

  inline void ModelEndSubmission() {
    for (int i = 0; i < submission_cache_.item_count; i++) {
      item_to_bone_matrices_offset_[{submission_cache_.model, i}] =
          num_bone_matrices_;
      num_bone_matrices_ += submission_cache_.num_bone_matrices;
    }

    submission_cache_.model = nullptr;
    submission_cache_.item_count = 0;
    submission_cache_.num_bone_matrices = 0;
  }

  void PrepareForDraw();

  void DrawDepthForShadow(
      Shadow *shadow,
      const std::vector<RenderTargetParameter> &render_target_params);
  void Draw(Camera *camera, LightSources *light_sources,
            ShadowSources *shadow_sources, OITRenderQuad *oit_render_quad,
            bool deferred_shading, bool default_shading, bool force_pbr,
            const std::vector<RenderTargetParameter> &render_target_params);

 private:
  void CheckRenderTargetParameter(
      const std::vector<RenderTargetParameter> &render_target_params);
  void UpdateBuffers(
      const std::vector<RenderTargetParameter> &render_target_params);
  void BindBuffers();

  struct Material {
    int32_t textures[7];
    alignas(16) glm::vec3 ka;
    alignas(16) glm::vec3 kd;
    alignas(16) glm::vec3 ks;
    alignas(16) float shininess;
    bool bind_metalness_and_diffuse_roughness;
  };

  // counters
  uint32_t num_instances_ = 0, num_bone_matrices_ = 0;

  // vertices, indices and commands
  std::vector<VertexWithBones> vertices_;
  std::vector<uint32_t> indices_;
  std::vector<DrawElementsIndirectCommand> commands_;

  // the buffers corresponding to the SSBOs
  std::vector<glm::mat4> model_matrices_, bone_matrices_;
  std::vector<uint32_t> bone_matrices_offset_;
  std::vector<uint32_t> has_bone_, animated_;
  std::vector<glm::mat4> transforms_;
  std::vector<glm::vec4> clip_planes_;
  std::vector<Material> materials_;
  std::vector<Texture> textures_;
  std::vector<uint64_t> texture_handles_;

  // buffers
  uint32_t commands_buffer_, vao_, ebo_, vbo_;

  // SSBOs
  std::unique_ptr<SSBO> model_matrices_ssbo_, bone_matrices_ssbo_,
      bone_matrices_offset_ssbo_, animated_ssbo_, transforms_ssbo_,
      clip_planes_ssbo_, materials_ssbo_, textures_ssbo_;

  struct SubmissionCache {
    Model *model;
    uint32_t item_count;
    uint32_t num_bone_matrices;
  } submission_cache_;

  // for model to locate array offset
  std::map<std::pair<Model *, uint32_t>, uint32_t> item_to_instance_offset_,
      item_to_bone_matrices_offset_;
  std::map<Model *, uint32_t> model_to_item_count_;
};

#endif