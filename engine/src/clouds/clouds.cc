#include "clouds/clouds.h"

#include <glad/glad.h>

#include "light_sources.h"
#include "texture_manager.h"
#include "utils.h"

void Clouds::Allocate(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  frag_color_texture_id_ = TextureManager::AllocateTexture(
      width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, false);

  fbo_.reset(new FrameBufferObject(width, height));
}

void Clouds::Deallocate() { glDeleteTextures(1, &frag_color_texture_id_); }

Clouds::Clouds(uint32_t width, uint32_t height) {
  shader_.reset(new Shader({{GL_COMPUTE_SHADER, Clouds::kCsSource}}));
  screen_space_shader_ = ScreenSpaceShader(kFsSource);
  glGenVertexArrays(1, &vao_);
  Allocate(width, height);
}

void Clouds::Resize(uint32_t width, uint32_t height) {
  Deallocate();
  Allocate(width, height);
}

void Clouds::Draw(Camera *camera, LightSources *light_sources) {
  shader_->Use();
  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));
  shader_->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  shader_->SetUniform<glm::mat4>(
      "uInvViewProjectionMatrix",
      glm::inverse(camera->projection_matrix() * camera->view_matrix()));
  glBindImageTexture(0, frag_color_texture_id_, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA32F);
  shader_->SetUniform<int32_t>("uFragColor", 0);
  glDispatchCompute((width_ + 3) / 4, (height_ + 3) / 4, 1);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  glBindVertexArray(vao_);
  screen_space_shader_->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fbo_->color_texture_id());
  screen_space_shader_->SetUniform<int32_t>("uScreenTexture", 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, frag_color_texture_id_);
  screen_space_shader_->SetUniform<int32_t>("uCloudTexture", 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, fbo_->depth_texture_id());
  screen_space_shader_->SetUniform<int32_t>("uDepthTexture", 2);
  screen_space_shader_->SetUniform<glm::vec2>("uScreenSize",
                                              glm::vec2(width_, height_));
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

const std::string Clouds::kCsSource = R"(
#version 430

layout (local_size_x = 4, local_size_y = 4, local_size_z = 1) in;
)" + LightSources::FsSource() + R"(

layout (rgba32f) uniform image2D uFragColor;

uniform vec2 uScreenSize;
uniform vec3 uCameraPosition;
uniform mat4 uInvViewProjectionMatrix;

void main() {
    if (gl_GlobalInvocationID.x >= uScreenSize.x || gl_GlobalInvocationID.y >= uScreenSize.y) {
        return;
    }
    vec4 clipSpaceCoord = vec4(vec2(gl_GlobalInvocationID.xy) / uScreenSize * 2 - 1, 1, 1);
    vec4 worldCoord = uInvViewProjectionMatrix * clipSpaceCoord;
    worldCoord /= worldCoord.w;
    vec3 rayDirection = normalize(vec3(worldCoord) - uCameraPosition);

    imageStore(uFragColor, ivec2(gl_GlobalInvocationID), vec4((rayDirection.xyz + 1) * 0.5, 1));
}
)";

const std::string Clouds::kFsSource = R"(
#version 410 core

uniform sampler2D uScreenTexture;
uniform sampler2D uCloudTexture;
uniform sampler2D uDepthTexture;
uniform vec2 uScreenSize;

out vec4 fragColor;

void main() {
    vec2 texCoord = gl_FragCoord.xy / uScreenSize;

    vec4 cloud = texture(uCloudTexture, texCoord);
	  vec4 background = texture(uScreenTexture, texCoord);
	  float a = texture(uDepthTexture, texCoord).r < 1.0 ? 0.0 : 1.0;
	  fragColor = mix(background, cloud, a);
}
)";
