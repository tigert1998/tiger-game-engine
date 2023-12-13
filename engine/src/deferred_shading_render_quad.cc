#include "deferred_shading_render_quad.h"

#include <glad/glad.h>

void DeferredShadingRenderQuad::ClearTextures() {
  uint8_t zero = 0;
  glClearTexImage(fbo_->color_texture(7).id(), 0, GL_RED_INTEGER,
                  GL_UNSIGNED_BYTE, &zero);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void DeferredShadingRenderQuad::Allocate(uint32_t width, uint32_t height) {
  Texture ka(width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT, GL_NEAREST,
             GL_NEAREST, {}, false);
  Texture kd(width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT, GL_NEAREST,
             GL_NEAREST, {}, false);
  Texture ks_and_shininess(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                           GL_REPEAT, GL_NEAREST, GL_NEAREST, {}, false);
  Texture albedo(width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture metallic_and_roughness_and_ao(width, height, GL_RGB16F, GL_RGB,
                                        GL_FLOAT, GL_REPEAT, GL_NEAREST,
                                        GL_NEAREST, {}, false);
  Texture normal(width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture position_and_alpha(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                             GL_REPEAT, GL_NEAREST, GL_NEAREST, {}, false);
  Texture flag(width, height, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
               GL_REPEAT, GL_NEAREST, GL_NEAREST, {}, false);
  std::vector<Texture> color_textures;
  color_textures.push_back(std::move(ka));
  color_textures.push_back(std::move(kd));
  color_textures.push_back(std::move(ks_and_shininess));
  color_textures.push_back(std::move(albedo));
  color_textures.push_back(std::move(metallic_and_roughness_and_ao));
  color_textures.push_back(std::move(normal));
  color_textures.push_back(std::move(position_and_alpha));
  color_textures.push_back(std::move(flag));
  Texture depth_texture(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                        GL_FLOAT, GL_REPEAT, GL_LINEAR, GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(color_textures, depth_texture));
}

DeferredShadingRenderQuad::DeferredShadingRenderQuad(uint32_t width,
                                                     uint32_t height) {
  Resize(width, height);

  if (kShader == nullptr) {
    std::map<std::string, std::any> constants = {
        {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
    };
    kShader = ScreenSpaceShader(kFsSource, constants);
    glGenVertexArrays(1, &vao_);
  }
  shader_ = kShader;
}

void DeferredShadingRenderQuad::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  Allocate(width, height);
}

void DeferredShadingRenderQuad::TwoPasses(
    const Camera* camera, LightSources* light_sources,
    ShadowSources* shadow_sources, const std::function<void()>& first_pass,
    const std::function<void()>& second_pass) {
  glViewport(0, 0, width_, height_);
  first_pass();

  glDisable(GL_BLEND);
  fbo_->Bind();
  ClearTextures();
  glViewport(0, 0, width_, height_);
  // Draw objects into G-Buffer
  second_pass();
  fbo_->Unbind();
  glEnable(GL_BLEND);

  glBindVertexArray(vao_);
  shader_->Use();
  shader_->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  shader_->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));

  shader_->SetUniformSampler("ka", fbo_->color_texture(0), 0);
  shader_->SetUniformSampler("kd", fbo_->color_texture(1), 1);
  shader_->SetUniformSampler("ksAndShininess", fbo_->color_texture(2), 2);
  shader_->SetUniformSampler("albedo", fbo_->color_texture(3), 3);
  shader_->SetUniformSampler("metallicAndRoughnessAndAo",
                             fbo_->color_texture(4), 4);
  shader_->SetUniformSampler("normal", fbo_->color_texture(5), 5);
  shader_->SetUniformSampler("positionAndAlpha", fbo_->color_texture(6), 6);
  shader_->SetUniformSampler("flag", fbo_->color_texture(7), 7);
  shader_->SetUniformSampler("depth", fbo_->depth_texture(), 8);

  int num_samplers = 9;

  light_sources->Set(shader_.get(), false);
  shadow_sources->Set(shader_.get(), &num_samplers);

  glDrawArrays(GL_POINTS, 0, 1);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

uint32_t DeferredShadingRenderQuad::vao_ = 0;

std::shared_ptr<Shader> DeferredShadingRenderQuad::kShader = nullptr;

const std::string DeferredShadingRenderQuad::kFsSource =
    R"(
#version 420 core

uniform mat4 uViewMatrix;
uniform vec3 uCameraPosition;
)" + LightSources::FsSource() +
    ShadowSources::FsSource() +
    R"(
out vec4 fragColor;

uniform sampler2D ka;
uniform sampler2D kd;
uniform sampler2D ksAndShininess;
uniform sampler2D albedo;
uniform sampler2D metallicAndRoughnessAndAo;
uniform sampler2D normal;
uniform sampler2D positionAndAlpha;
uniform usampler2D flag;
uniform sampler2D depth;

uniform vec2 uScreenSize;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    gl_FragDepth = texture(depth, coord).r;

    uint renderType = texture(flag, coord).r; 
    if (renderType == 0) {
        discard;
    } else if (renderType == 1) {
        float shadow = CalcShadow(
            texture(positionAndAlpha, coord).xyz, 
            texture(normal, coord).xyz
        );

        fragColor.rgb = CalcPhongLighting(
            texture(ka, coord).rgb,
            texture(kd, coord).rgb,
            texture(ksAndShininess, coord).rgb,
            texture(normal, coord).xyz,
            uCameraPosition,
            texture(positionAndAlpha, coord).xyz,
            texture(ksAndShininess, coord).w,
            shadow
        );

        fragColor.a = texture(positionAndAlpha, coord).a;
    } else if (renderType == 2) {
        float shadow = CalcShadow(
            texture(positionAndAlpha, coord).xyz,
            texture(normal, coord).xyz
        );

        fragColor.rgb = CalcPBRLighting(
            texture(albedo, coord).rgb, 
            texture(metallicAndRoughnessAndAo, coord).x, 
            texture(metallicAndRoughnessAndAo, coord).y, 
            texture(metallicAndRoughnessAndAo, coord).z,
            texture(normal, coord).xyz,
            uCameraPosition, 
            texture(positionAndAlpha, coord).xyz,
            shadow
        );

        fragColor.a = texture(positionAndAlpha, coord).a;
    }
}
)";
