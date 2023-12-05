#ifndef PHYSICS_COLLISION_MODEL_H_
#define PHYSICS_COLLISION_MODEL_H_

#include <PxPhysicsAPI.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>

class CollisionModel {
 public:
  CollisionModel() = delete;
  explicit CollisionModel(const std::string &path);
  void ExportToOBJ(const std::string &path) const;
  ~CollisionModel();
  physx::PxTriangleMesh *mesh(physx::PxPhysics *physics) {
    if (mesh_ == nullptr) {
      mesh_ = CreatePxTriangleMesh(physics, false, false, false, true);
    }
    return mesh_;
  }

 private:
  std::vector<physx::PxVec3> vertices_;
  std::vector<uint32_t> indices_;
  physx::PxTriangleMesh *mesh_ = nullptr;

  void RecursivelyInitNodes(const aiScene *scene, const aiNode *node,
                            glm::mat4 parent_transform);
  void AppendMesh(const aiMesh *mesh, glm::mat4 transform);
  physx::PxTriangleMesh *CreatePxTriangleMesh(
      physx::PxPhysics *physics, bool skip_mesh_cleanup, bool skip_edge_data,
      bool cooking_performance, bool mesh_size_perf_tradeoff) const;
};

#endif