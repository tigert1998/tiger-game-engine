#include "grass/blade.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/core.h>
#include <glad/glad.h>

#include <assimp/Importer.hpp>

#include "glsl_common.h"
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

const std::string Blade::kVsSource = R"(
#version 410 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in mat4 aBladeTransform;
layout (location = 7) in vec3 aBladePosition;
layout (location = 8) in vec2 aBladeTexCoord;

out vec3 vPosition;
out vec2 vTexCoord;
out vec3 vNormal;

uniform vec2 uWindFrequency;
uniform float uTime;
uniform sampler2D uDistortionTexture;

uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
)" + GLSLCommon::Source() + R"(

mat4 CalcWindRotation() {
    vec2 uv = aBladeTexCoord + uWindFrequency * uTime;
    vec2 sampled = texture(uDistortionTexture, uv).xy * 2.0f - 1.0f;
    return Rotate(
        vec3(sampled.x, 0, sampled.y),
        PI * 0.5 * length(sampled)
    );
}

void main() {
    mat4 modelMatrix = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(aBladePosition, 1.0)
    ) * CalcWindRotation() * aBladeTransform;
    gl_Position = uProjectionMatrix * uViewMatrix * modelMatrix * vec4(aPosition, 1);
    vPosition = vec3(modelMatrix * vec4(aPosition, 1));
    vTexCoord = aTexCoord;
    vNormal = vec3(transpose(inverse(modelMatrix)) * vec4(aNormal, 0));
}
)";

const std::string Blade::kFsSource = R"(
#version 420 core

const float zero = 1e-6;

uniform vec3 uCameraPosition;

in vec3 vPosition;
in vec2 vTexCoord;
in vec3 vNormal;
)" + LightSources::FsSource() + R"(

out vec4 fragColor;

vec4 CalcFragColor() {
    vec3 lightGreen = vec3(139.0, 205.0, 80.0) / 255.0;
    vec3 darkGreen = vec3(53.0, 116.0, 32.0) / 255.0;
    vec3 green = mix(darkGreen, lightGreen, vTexCoord.y);

    float facing = dot(vPosition - uCameraPosition, -vNormal);
    vec3 normal = facing > 0 ? vNormal : -vNormal;

    vec3 color = CalcPhongLighting(
        green, green, vec3(zero),
        normal, uCameraPosition, vPosition,
        0, 0
    );

    return vec4(color, 1.0);
}

void main() {
    fragColor = CalcFragColor();
}
)";

Blade::Blade() {
  shader_.reset(new Shader(Blade::kVsSource, Blade::kFsSource, {}));

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
