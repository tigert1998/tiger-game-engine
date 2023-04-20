#ifndef MODEL_H_
#define MODEL_H_

#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "light_sources.h"
#include "mesh.h"
#include "shader.h"

class Model {
 public:
  Model() = delete;
  Model(const std::string &path);
  ~Model();
  void Draw(Camera *camera_ptr, LightSources *light_sources,
            glm::mat4 model_matrix);
  void Draw(Camera *camera_ptr, LightSources *light_sources,
            const std::vector<glm::mat4> &model_matrices, glm::vec4 clip_plane);
  void Draw(uint32_t animation_id, double time, Camera *camera_ptr,
            LightSources *light_sources, glm::mat4 model_matrix,
            glm::vec4 clip_plane);
  int NumAnimations() const;
  void set_default_shading(bool default_shading);
  inline bool default_shading() { return default_shading_; }

  inline glm::vec3 min() { return min_; }
  inline glm::vec3 max() { return max_; }

 private:
  std::string directory_path_;
  std::vector<std::shared_ptr<Mesh>> mesh_ptrs_;
  const aiScene *scene_;
  std::shared_ptr<Shader> shader_ptr_;
  Namer bone_namer_;
  uint32_t vbo_;
  glm::vec3 min_, max_;
  std::vector<glm::mat4> bone_matrices_, bone_offsets_;
  std::map<std::pair<uint32_t, std::string>, uint32_t> animation_channel_map_;
  bool default_shading_ = false;

  void RecursivelyInitNodes(aiNode *node, glm::mat4 parent_transform);
  void RecursivelyUpdateBoneMatrices(int animation_id, aiNode *node,
                                     glm::mat4 transform, double ticks);

  static glm::mat4 InterpolateTranslationMatrix(aiVectorKey *keys, uint32_t n,
                                                double ticks);
  static glm::mat4 InterpolateRotationMatrix(aiQuatKey *keys, uint32_t n,
                                             double ticks);
  static glm::mat4 InterpolateScalingMatrix(aiVectorKey *keys, uint32_t n,
                                            double ticks);

  void InternalDraw(bool animated, Camera *camera_ptr,
                    LightSources *light_sources,
                    const std::vector<glm::mat4> &model_matrices,
                    glm::vec4 clip_plane);

  static const std::string kVsSource;
  static const std::string kFsSource;
  static std::shared_ptr<Shader> kShader;
};

#endif