#ifndef MODEL_H_
#define MODEL_H_

#include <assimp/scene.h>

#include <map>
#include <memory>
#include <string>

#include "light_sources.h"
#include "mesh.h"
#include "multi_draw_indirect.h"
#include "shader.h"
#include "shadow_sources.h"

class Model {
  friend class MultiDrawIndirect;

 public:
  using TextureConfig = std::map<std::string, bool>;

  Model() = delete;
  Model(const std::string &path, MultiDrawIndirect *multi_draw_indirect,
        bool flip_y);
  int NumAnimations() const;
  ~Model();

 private:
  std::string directory_path_;
  bool flip_y_;
  const aiScene *scene_;
  std::map<std::pair<uint32_t, std::string>, uint32_t> animation_channel_map_;
  MultiDrawIndirect *multi_draw_indirect_;
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
  static std::unique_ptr<Shader> kShadowShader;
  static std::unique_ptr<Shader> kDeferredShadingShader;

  static const std::string kVsSource;
  static const std::string kGsShadowSource;
  static const std::string kFsSource;
  static const std::string kFsMainSource;
  static const std::string kFsOITMainSource;
  static const std::string kFsShadowSource;
  static const std::string kFsDeferredShadingMainSource;
};

#endif