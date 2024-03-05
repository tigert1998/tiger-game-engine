#ifndef MODEL_H_
#define MODEL_H_

#include <assimp/scene.h>
#include <stdint.h>

#include <assimp/Importer.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "light_sources.h"
#include "mesh.h"
#include "multi_draw_indirect.h"
#include "shader.h"
#include "shadows.h"

class Model {
  friend class MultiDrawIndirect;

 public:
  using TextureConfig = std::map<std::string, bool>;

  Model() = delete;
  Model(const std::filesystem::path &path, bool flip_y,
        bool split_large_meshes);
  int NumAnimations() const;
  double AnimationDurationInSeconds(int animation_id) const;
  void SubmitToMultiDrawIndirect(MultiDrawIndirect *multi_draw_indirect,
                                 uint32_t item_count);
  ~Model();
  uint32_t NumMeshes() const;
  Mesh *mesh(uint32_t index);

 private:
  Assimp::Importer importer_;
  std::filesystem::path directory_path_;
  bool flip_y_;
  const aiScene *scene_;
  std::map<std::pair<uint32_t, std::string>, uint32_t> animation_channel_map_;
  std::map<std::filesystem::path, Texture> textures_cache_;
  std::vector<std::unique_ptr<Mesh>> meshes_;
  Namer bone_namer_;
  std::vector<glm::mat4> bone_matrices_, bone_offsets_;

  static glm::mat4 InterpolateTranslationMatrix(aiVectorKey *keys, uint32_t n,
                                                double ticks);
  static glm::mat4 InterpolateRotationMatrix(aiQuatKey *keys, uint32_t n,
                                             double ticks);
  static glm::mat4 InterpolateScalingMatrix(aiVectorKey *keys, uint32_t n,
                                            double ticks);
  void InitAnimationChannelMap();
  void RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform);
  void RecursivelyUpdateBoneMatrices(int animation_id, aiNode *node,
                                     glm::mat4 transform, double ticks);
  void CompileShaders();

  static std::unique_ptr<Shader> kShader;
  static std::unique_ptr<Shader> kOITShader;
  static std::unique_ptr<Shader> kDirectionalShadowShader;
  static std::unique_ptr<Shader> kOmnidirectionalShadowShader;
  static std::unique_ptr<Shader> kDeferredShadingShader;
};

#endif