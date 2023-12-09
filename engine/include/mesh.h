#ifndef MESH_H_
#define MESH_H_

#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "shader.h"
#include "vertex.h"

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
  Mesh(const std::string &directory_path, aiMesh *mesh, const aiScene *scene,
       Namer &bone_namer, std::vector<glm::mat4> &bone_offsets, bool flip_y);

  ~Mesh();

  void Draw(Shader *shader_ptr, int num_instances, bool shadow,
            int32_t sampler_offset) const;

  inline std::string name() { return name_; }

  void AppendTransform(glm::mat4 transform);

  inline glm::vec3 center(glm::mat4 *bone_matrix) {
    glm::mat4 transform =
        bone_matrix == nullptr || !has_bone_ ? transforms_[0] : *bone_matrix;
    return transform * glm::vec4(center_, 1);
  }

  inline uint32_t vao() { return vao_; }

  inline glm::vec3 max() { return max_; }
  inline glm::vec3 min() { return min_; }

 private:
  std::vector<glm::mat4> transforms_;
  uint32_t vao_, vbo_, ebo_, indices_size_;
  std::string name_;
  bool has_bone_ = false;
  glm::vec3 center_, min_, max_;

  struct TextureRecord {
    bool enabled;  // whether the model contains this kind of texture
    uint32_t id;   // OpenGL texture ID
    int32_t op;    // ASSIMP aiTextureOp, not used yet in the shader
    float blend;
    glm::vec3 base_color;  // the base color for this kind of texture, not used
                           // yet in the shader
  };

#define REGISTER(name)                        \
  {                                           \
    #name, { false, 0, -1, -1, glm::vec3(0) } \
  }

  std::map<std::string, TextureRecord> textures_ = {
      REGISTER(DIFFUSE),           REGISTER(AMBIENT),
      REGISTER(SPECULAR),          REGISTER(NORMALS),
      REGISTER(METALNESS),         REGISTER(DIFFUSE_ROUGHNESS),
      REGISTER(AMBIENT_OCCLUSION),
  };

  struct PhongMaterial {
    glm::vec3 ka, kd, ks;
    float shininess;
  } material_;

#undef REGISTER
};

#endif