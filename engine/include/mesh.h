#ifndef MESH_H_
#define MESH_H_

#include <assimp/scene.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

#include "aabb.h"
#include "multi_draw_indirect.h"
#include "texture.h"

class Namer {
 public:
  Namer();
  uint32_t Name(const std::string &name);
  uint32_t total() const;
  std::map<std::string, uint32_t> &map();
  void Clear();

 private:
  std::map<std::string, uint32_t> map_;
  uint32_t total_;
};

class Mesh {
 public:
  explicit Mesh(const std::filesystem::path &directory_path, aiMesh *mesh,
                const aiScene *scene, Namer *bone_namer,
                std::vector<glm::mat4> *bone_offsets,
                std::map<std::filesystem::path, Texture> *textures_cache,
                bool flip_y, glm::mat4 transform);
  void SubmitToMultiDrawIndirect(MultiDrawIndirect *multi_draw_indirect);
  ~Mesh();
  MaterialParameters *material_params();

 private:
  AABB aabb_;
  glm::mat4 transform_;
  std::string name_;
  bool has_bone_ = false;

  std::vector<TextureRecord> textures_;
  std::vector<VertexWithBones> vertices_;
  std::vector<std::vector<uint32_t>> indices_;  // LODs
  MaterialParameters material_params_;

  void AddVerticesIndicesAndBones(aiMesh *mesh, Namer *bone_namer,
                                  std::vector<glm::mat4> *bone_offsets);

  void MakeTexturesResidentOrNot(bool resident);
};

#endif