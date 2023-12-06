#include "physics/collision_model.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <glog/logging.h>

#include <assimp/Importer.hpp>

#include "utils.h"

CollisionModel::CollisionModel(const std::string &path) {
  LOG(INFO) << "loading collision model at: \"" << path << "\"";
  auto scene = aiImportFile(path.c_str(), aiProcess_GlobalScale |
                                              aiProcess_CalcTangentSpace |
                                              aiProcess_Triangulate);

  RecursivelyInitNodes(scene, scene->mRootNode, glm::mat4(1));

  aiReleaseImport(scene);
}

CollisionModel::~CollisionModel() {
  if (mesh_ != nullptr) {
    mesh_->release();
  }
  mesh_ = nullptr;
}

void CollisionModel::AppendMesh(const aiMesh *mesh, glm::mat4 transform) {
  uint32_t num_vertices = vertices_.size();
  for (int i = 0; i < mesh->mNumVertices; i++) {
    aiVector3D vertex = mesh->mVertices[i];
    glm::vec4 new_vertex =
        transform * glm::vec4(vertex.x, vertex.y, vertex.z, 1);
    new_vertex /= new_vertex.w;
    vertices_.emplace_back(new_vertex.x, new_vertex.y, new_vertex.z);
  }
  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    indices_.push_back(num_vertices + face.mIndices[0]);
    indices_.push_back(num_vertices + face.mIndices[1]);
    indices_.push_back(num_vertices + face.mIndices[2]);
  }
}

void CollisionModel::RecursivelyInitNodes(const aiScene *scene,
                                          const aiNode *node,
                                          glm::mat4 parent_transform) {
  auto node_transform = Mat4FromAimatrix4x4(node->mTransformation);
  auto transform = parent_transform * node_transform;

  LOG(INFO) << "initializing node \"" << node->mName.C_Str() << "\"";
  for (int i = 0; i < node->mNumMeshes; i++) {
    int id = node->mMeshes[i];
    auto mesh = scene->mMeshes[id];
    AppendMesh(mesh, transform);
  }

  for (int i = 0; i < node->mNumChildren; i++) {
    RecursivelyInitNodes(scene, node->mChildren[i], transform);
  }
}

void CollisionModel::ExportToOBJ(const std::string &path) const {
  FILE *fp = fopen(path.c_str(), "w");
  for (int i = 0; i < vertices_.size(); i++) {
    fprintf(fp, "v %.6f %.6f %.6f\n", vertices_[i].x, vertices_[i].y,
            vertices_[i].z);
  }
  for (int i = 0; i < indices_.size(); i += 3) {
    fprintf(fp, "f %d %d %d\n", indices_[i] + 1, indices_[i + 1] + 1,
            indices_[i + 2] + 1);
  }
  fclose(fp);
}

physx::PxTriangleMesh *CollisionModel::CreatePxTriangleMesh(
    physx::PxPhysics *physics, bool skip_mesh_cleanup, bool skip_edge_data,
    bool cooking_performance, bool mesh_size_perf_tradeoff) const {
  physx::PxTriangleMeshDesc mesh_desc;
  mesh_desc.points.count = vertices_.size();
  mesh_desc.points.data = vertices_.data();
  mesh_desc.points.stride = sizeof(physx::PxVec3);
  mesh_desc.triangles.count = indices_.size() / 3;
  mesh_desc.triangles.data = indices_.data();
  mesh_desc.triangles.stride = 3 * sizeof(indices_[0]);

  physx::PxTolerancesScale scale;
  physx::PxCookingParams params(scale);

  // Create BVH33 midphase
  params.midphaseDesc = physx::PxMeshMidPhase::eBVH33;

  // setup common cooking params
  // we suppress the triangle mesh remap table computation to gain some speed,
  // as we will not need it
  // in this snippet
  params.suppressTriangleMeshRemapTable = true;

  // If DISABLE_CLEAN_MESH is set, the mesh is not cleaned during the cooking.
  // The input mesh must be valid. The following conditions are true for a valid
  // triangle mesh :
  //  1. There are no duplicate vertices(within specified
  //  vertexWeldTolerance.See PxCookingParams::meshWeldTolerance)
  //  2. There are no large triangles(within specified PxTolerancesScale.)
  // It is recommended to run a separate validation check in debug/checked
  // builds, see below.

  if (!skip_mesh_cleanup)
    params.meshPreprocessParams &=
        ~static_cast<physx::PxMeshPreprocessingFlags>(
            physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
  else
    params.meshPreprocessParams |=
        physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;

  // If eDISABLE_ACTIVE_EDGES_PRECOMPUTE is set, the cooking does not compute
  // the active (convex) edges, and instead marks all edges as active. This
  // makes cooking faster but can slow down contact generation. This flag may
  // change the collision behavior, as all edges of the triangle mesh will now
  // be considered active.
  if (!skip_edge_data)
    params.meshPreprocessParams &=
        ~static_cast<physx::PxMeshPreprocessingFlags>(
            physx::PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE);
  else
    params.meshPreprocessParams |=
        physx::PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;

  // The COOKING_PERFORMANCE flag for BVH33 midphase enables a fast cooking path
  // at the expense of somewhat lower quality BVH construction.
  if (cooking_performance)
    params.midphaseDesc.mBVH33Desc.meshCookingHint =
        physx::PxMeshCookingHint::eCOOKING_PERFORMANCE;
  else
    params.midphaseDesc.mBVH33Desc.meshCookingHint =
        physx::PxMeshCookingHint::eSIM_PERFORMANCE;

  // If mesh_size_perf_tradeoff is set to true, smaller mesh cooked mesh is
  // produced. The mesh size/performance trade-off is controlled by setting the
  // meshSizePerformanceTradeOff from 0.0f (smaller mesh) to 1.0f (larger mesh).
  if (mesh_size_perf_tradeoff) {
    params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff = 0.0f;
  } else {
    // using the default value
    params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff = 0.55f;
  }

#if defined(PX_CHECKED) || defined(PX_DEBUG)
  // If DISABLE_CLEAN_MESH is set, the mesh is not cleaned during the cooking.
  // We should check the validity of provided triangles in debug/checked builds
  // though.
  if (skip_mesh_cleanup) {
    PX_ASSERT(PxValidateTriangleMesh(params, mesh_desc));
  }
#endif  // DEBUG

  return PxCreateTriangleMesh(params, mesh_desc,
                              physics->getPhysicsInsertionCallback());
  // The cooked mesh may either be saved to a stream for later loading, or
  // inserted directly into PxPhysics.

  // physx::PxDefaultMemoryOutputStream out_buf;
  // PxCookTriangleMesh(params, mesh_desc, out_buf);

  // physx::PxDefaultMemoryInputData stream(out_buf.getData(),
  //                                        out_buf.getSize());
  // return physics->createTriangleMesh(stream);
}