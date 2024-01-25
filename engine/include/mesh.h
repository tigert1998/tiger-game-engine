#ifndef MESH_H_
#define MESH_H_

#include <assimp/scene.h>

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
  explicit Mesh(const std::string &directory_path, aiMesh *mesh,
                const aiScene *scene, Namer *bone_namer,
                std::vector<glm::mat4> *bone_offsets, bool flip_y,
                MultiDrawIndirect *multi_draw_indirect);
  void AppendTransform(glm::mat4 transform);
  void SubmitToMultiDrawIndirect();

 private:
  AABB aabb_;
  std::vector<glm::mat4> transforms_;
  std::string name_;
  bool has_bone_ = false;
  MultiDrawIndirect *multi_draw_indirect_;

  std::vector<TextureRecord> textures_;
  std::vector<VertexWithBones> vertices_;
  std::vector<std::vector<uint32_t>> indices_;  // LODs
  PhongMaterial material_;
};

#endif