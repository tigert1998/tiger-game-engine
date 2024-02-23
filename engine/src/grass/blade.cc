#include "grass/blade.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/core.h>
#include <glad/glad.h>

#include <assimp/Importer.hpp>

#include "light_sources.h"
#include "vertex.h"

const std::string Blade::kOBJSource = R"(
v -0.5 0 0
v 0.5 0 0
v -0.4 0.35 0
v 0.4 0.35 0
v -0.25 0.72 0
v 0.25 0.72 0
v 0 1 0
# we only use tex coords' y axis
vt -0.5 0
vt 0.5 0
vt -0.4 0.35
vt 0.4 0.35
vt -0.25 0.72
vt 0.25 0.72
vt 0 1
f 1/1 2/2 4/4 3/3
f 3/3 4/4 6/6 5/5
f 5/5 6/6 7/7
)";

Blade::Blade() {
  shader_.reset(new Shader(
      "grass/grass.vert", "grass/grass.frag",
      {
          {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
          {"AMBIENT_LIGHT_BINDING", std::any(AmbientLight::GLSL_BINDING)},
          {"DIRECTIONAL_LIGHT_BINDING",
           std::any(DirectionalLight::GLSL_BINDING)},
          {"POINT_LIGHT_BINDING", std::any(PointLight::GLSL_BINDING)},
      }));

  auto scene = aiImportFileFromMemory(
      Blade::kOBJSource.c_str(), Blade::kOBJSource.size(),
      aiProcess_GlobalScale | aiProcess_CalcTangentSpace |
          aiProcess_Triangulate | aiProcess_GenNormals,
      "");
  if (scene->mNumMeshes != 1) {
    fmt::print(stderr, "[error] scene->mNumMeshes != 1\n");
    exit(1);
  }
  auto mesh = scene->mMeshes[0];

  using VertexType = Vertex<0, true>;

  std::vector<VertexType> vertices;
  std::vector<uint32_t> indices;
  vertices.reserve(mesh->mNumVertices);
  indices.reserve(mesh->mNumFaces * 3);

  for (int i = 0; i < mesh->mNumVertices; i++) {
    VertexType vertex;
    vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                mesh->mVertices[i].z);
    vertex.tex_coord =
        glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
    vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                              mesh->mNormals[i].z);
    vertices.push_back(vertex);
  }

  for (int i = 0; i < mesh->mNumFaces; i++) {
    auto face = mesh->mFaces[i];
    for (int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }
  indices_size_ = (uint32_t)indices.size();

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ebo_);

  glBindVertexArray(vao_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VertexType) * vertices.size(),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexType),
                        (void *)offsetof(VertexType, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexType),
                        (void *)offsetof(VertexType, tex_coord));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexType),
                        (void *)offsetof(VertexType, normal));

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
