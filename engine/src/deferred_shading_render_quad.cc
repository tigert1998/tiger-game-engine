#include "deferred_shading_render_quad.h"

#include <glad/glad.h>

#include <random>

#include "utils.h"

void DeferredShadingRenderQuad::InitSSAO() {
  std::uniform_real_distribution<float> dis(0.0, 1.0);
  std::default_random_engine generator;
  std::vector<glm::vec4> ssao_kernel;
  for (unsigned int i = 0; i < 64; ++i) {
    glm::vec3 sample(dis(generator) * 2.0 - 1.0, dis(generator) * 2.0 - 1.0,
                     dis(generator));
    sample = glm::normalize(sample);
    sample *= dis(generator);
    float scale = float(i) / 64.0f;
    scale = glm::mix(0.1f, 1.0f, scale * scale);
    sample *= scale;
    ssao_kernel.push_back(glm::vec4(sample, 0));
  }
  ssao_kernel_ssbo_.reset(new SSBO(ssao_kernel.size() * sizeof(ssao_kernel[0]),
                                   ssao_kernel.data(), GL_STATIC_DRAW, 0));

  std::vector<glm::vec3> ssao_noise;
  for (unsigned int i = 0; i < 16; i++) {
    glm::vec3 noise(dis(generator) * 2.0 - 1.0, dis(generator) * 2.0 - 1.0,
                    0.0f);
    ssao_noise.push_back(noise);
  }

  ssao_noise_texture_ =
      Texture(ssao_noise.data(), 4, 4, GL_RGBA16F, GL_RGB, GL_FLOAT, GL_REPEAT,
              GL_NEAREST, GL_NEAREST, {}, false);
}

void DeferredShadingRenderQuad::ClearTextures() {
  uint8_t zero = 0;
  glClearTexImage(fbo_->color_texture(7).id(), 0, GL_RED_INTEGER,
                  GL_UNSIGNED_BYTE, &zero);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void DeferredShadingRenderQuad::Allocate(uint32_t width, uint32_t height) {
  Texture ka(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
             GL_NEAREST, GL_NEAREST, {}, false);
  Texture kd(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
             GL_NEAREST, GL_NEAREST, {}, false);
  Texture ks_and_shininess(nullptr, width, height, GL_RGBA16F, GL_RGBA,
                           GL_FLOAT, GL_REPEAT, GL_NEAREST, GL_NEAREST, {},
                           false);
  Texture albedo(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture metallic_and_roughness_and_ao(nullptr, width, height, GL_RGB16F,
                                        GL_RGB, GL_FLOAT, GL_REPEAT, GL_NEAREST,
                                        GL_NEAREST, {}, false);
  Texture normal(nullptr, width, height, GL_RGB16F, GL_RGB, GL_FLOAT, GL_REPEAT,
                 GL_NEAREST, GL_NEAREST, {}, false);
  Texture position_and_alpha(nullptr, width, height, GL_RGBA16F, GL_RGBA,
                             GL_FLOAT, GL_REPEAT, GL_NEAREST, GL_NEAREST, {},
                             false);
  Texture flag(nullptr, width, height, GL_R8UI, GL_RED_INTEGER,
               GL_UNSIGNED_BYTE, GL_REPEAT, GL_NEAREST, GL_NEAREST, {}, false);
  std::vector<Texture> color_textures;
  color_textures.push_back(std::move(ka));
  color_textures.push_back(std::move(kd));
  color_textures.push_back(std::move(ks_and_shininess));
  color_textures.push_back(std::move(albedo));
  color_textures.push_back(std::move(metallic_and_roughness_and_ao));
  color_textures.push_back(std::move(normal));
  color_textures.push_back(std::move(position_and_alpha));
  color_textures.push_back(std::move(flag));
  Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                        GL_DEPTH_COMPONENT, GL_FLOAT, GL_REPEAT, GL_LINEAR,
                        GL_LINEAR, {}, false);
  fbo_.reset(new FrameBufferObject(color_textures, depth_texture));

  // SSAO
  Texture ssao_color_texture(nullptr, width, height, GL_RED, GL_RED, GL_FLOAT,
                             GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST, {},
                             false);
  std::vector<Texture> ssao_color_textures;
  ssao_color_textures.push_back(std::move(ssao_color_texture));
  ssao_fbo_.reset(new FrameBufferObject(ssao_color_textures));

  Texture ssao_blur_color_texture(nullptr, width, height, GL_RED, GL_RED,
                                  GL_FLOAT, GL_CLAMP_TO_EDGE, GL_NEAREST,
                                  GL_NEAREST, {}, false);
  std::vector<Texture> ssao_blur_color_textures;
  ssao_blur_color_textures.push_back(std::move(ssao_blur_color_texture));
  ssao_blur_fbo_.reset(new FrameBufferObject(ssao_blur_color_textures));
}

DeferredShadingRenderQuad::DeferredShadingRenderQuad(uint32_t width,
                                                     uint32_t height) {
  InitSSAO();
  Resize(width, height);

  if (kShader == nullptr && kSSAOShader == nullptr &&
      kSSAOBlurShader == nullptr) {
    std::map<std::string, std::any> defines = {
        {"NUM_CASCADES", std::any(DirectionalShadow::NUM_CASCADES)},
    };
    kShader = ScreenSpaceShader(kFsSource, defines);
    kSSAOShader = ScreenSpaceShader(kSSAOFsSource, {});
    kSSAOBlurShader = ScreenSpaceShader(kSSAOBlurFsSource, {});
    glGenVertexArrays(1, &vao_);
  }
}

void DeferredShadingRenderQuad::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  Allocate(width, height);
}

void DeferredShadingRenderQuad::TwoPasses(
    const Camera* camera, LightSources* light_sources,
    ShadowSources* shadow_sources, bool enable_ssao,
    const std::function<void()>& first_pass,
    const std::function<void()>& second_pass,
    const FrameBufferObject* dest_fbo) {
  if (dest_fbo != nullptr) {
    dest_fbo->Bind();
  }
  glViewport(0, 0, width_, height_);
  first_pass();
  if (dest_fbo != nullptr) {
    dest_fbo->Unbind();
  }

  glDisable(GL_BLEND);
  fbo_->Bind();
  ClearTextures();
  glViewport(0, 0, width_, height_);
  // Draw objects into G-Buffer
  second_pass();
  fbo_->Unbind();
  glEnable(GL_BLEND);

  if (enable_ssao) {
    glDisable(GL_BLEND);
    // SSAO
    ssao_fbo_->Bind();
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vao_);
    kSSAOShader->Use();
    kSSAOShader->SetUniformSampler("normal", fbo_->color_texture(5), 0);
    kSSAOShader->SetUniformSampler("positionAndAlpha", fbo_->color_texture(6),
                                   1);
    kSSAOShader->SetUniformSampler("uNoiseTexture", ssao_noise_texture_, 2);
    kSSAOShader->SetUniform<glm::vec2>("uScreenSize",
                                       glm::vec2(width_, height_));
    kSSAOShader->SetUniform<float>("uRadius", 0.5);
    kSSAOShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
    kSSAOShader->SetUniform<glm::mat4>("uProjectionMatrix",
                                       camera->projection_matrix());
    ssao_kernel_ssbo_->BindBufferBase();
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    ssao_fbo_->Unbind();

    // SSAO blur
    ssao_blur_fbo_->Bind();
    glViewport(0, 0, width_, height_);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vao_);
    kSSAOBlurShader->Use();
    kSSAOBlurShader->SetUniform<glm::vec2>("uScreenSize",
                                           glm::vec2(width_, height_));
    kSSAOBlurShader->SetUniformSampler("uInput", ssao_fbo_->color_texture(0),
                                       0);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    ssao_blur_fbo_->Unbind();
    glEnable(GL_BLEND);
  }

  glBindVertexArray(vao_);
  kShader->Use();
  kShader->SetUniform<glm::mat4>("uViewMatrix", camera->view_matrix());
  kShader->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  kShader->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));
  kShader->SetUniform<int32_t>("uEnableSSAO", enable_ssao);

  kShader->SetUniformSampler("ka", fbo_->color_texture(0), 0);
  kShader->SetUniformSampler("kd", fbo_->color_texture(1), 1);
  kShader->SetUniformSampler("ksAndShininess", fbo_->color_texture(2), 2);
  kShader->SetUniformSampler("albedo", fbo_->color_texture(3), 3);
  kShader->SetUniformSampler("metallicAndRoughnessAndAo",
                             fbo_->color_texture(4), 4);
  kShader->SetUniformSampler("normal", fbo_->color_texture(5), 5);
  kShader->SetUniformSampler("positionAndAlpha", fbo_->color_texture(6), 6);
  kShader->SetUniformSampler("flag", fbo_->color_texture(7), 7);
  kShader->SetUniformSampler("depth", fbo_->depth_texture(), 8);

  if (enable_ssao) {
    kShader->SetUniformSampler("uSSAO", ssao_blur_fbo_->color_texture(0), 9);
  } else {
    kShader->SetUniformSampler("uSSAO", Texture::Empty(GL_TEXTURE_2D), 9);
  }

  int num_samplers = 10;

  light_sources->Set(kShader.get(), false);
  shadow_sources->Set(kShader.get(), &num_samplers);

  if (dest_fbo != nullptr) {
    dest_fbo->Bind();
  }
  glDrawArrays(GL_POINTS, 0, 1);
  if (dest_fbo != nullptr) {
    dest_fbo->Unbind();
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

uint32_t DeferredShadingRenderQuad::vao_ = 0;

std::unique_ptr<Shader> DeferredShadingRenderQuad::kShader = nullptr;
std::unique_ptr<Shader> DeferredShadingRenderQuad::kSSAOShader = nullptr;
std::unique_ptr<Shader> DeferredShadingRenderQuad::kSSAOBlurShader = nullptr;

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
uniform sampler2D uSSAO;

uniform vec2 uScreenSize;
uniform bool uEnableSSAO;

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
    } else if (renderType == 2) {
        float shadow = CalcShadow(
            texture(positionAndAlpha, coord).xyz,
            texture(normal, coord).xyz
        );

        float ao = texture(metallicAndRoughnessAndAo, coord).z;
        if (uEnableSSAO) {
            ao = texture(uSSAO, coord).r;
        }

        fragColor.rgb = CalcPBRLighting(
            texture(albedo, coord).rgb, 
            texture(metallicAndRoughnessAndAo, coord).x, 
            texture(metallicAndRoughnessAndAo, coord).y,
            ao,
            texture(normal, coord).xyz,
            uCameraPosition,
            texture(positionAndAlpha, coord).xyz,
            shadow
        );
    }

    fragColor.a = 1;
}
)";

const std::string DeferredShadingRenderQuad::kSSAOFsSource = R"(
#version 430 core

uniform sampler2D normal;
uniform sampler2D positionAndAlpha;
uniform sampler2D uNoiseTexture;
uniform vec2 uScreenSize;
uniform float uRadius;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

layout (std430, binding = 0) buffer kernelsBuffer {
    vec4 kernels[];
};

out float fragColor;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec3 position = texture(positionAndAlpha, coord).xyz;
    vec3 normalVector = texture(normal, coord).xyz;
    vec3 random = texture(uNoiseTexture, coord * uScreenSize / 4.0f).xyz;
    vec3 tangent = normalize(random - normalVector * dot(random, normalVector));
    vec3 bitangent = cross(normalVector, tangent);
    mat3 TBN = mat3(tangent, bitangent, normalVector);

    const int kernelSize = 64;
    float viewSpacePositionDepth = (uViewMatrix * vec4(position, 1)).z;

    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; i++) {
        vec3 samplePosition = TBN * kernels[i].xyz;
        samplePosition = position + samplePosition * uRadius;
        samplePosition = (uViewMatrix * vec4(samplePosition, 1)).xyz;

        vec4 offset = uProjectionMatrix * vec4(samplePosition, 1);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        float sampleDepth = (uViewMatrix * vec4(texture(positionAndAlpha, offset.xy).xyz, 1)).z;

        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(viewSpacePositionDepth - sampleDepth));
        const float bias = 0.025;
        occlusion += (sampleDepth >= samplePosition.z + bias ? 1.0 : 0.0);  
    }

    fragColor = 1.0 - (occlusion / kernelSize);
}
)";

const std::string DeferredShadingRenderQuad::kSSAOBlurFsSource = R"(
#version 330 core
out float fragColor;

uniform vec2 uScreenSize;
uniform sampler2D uInput;

void main() {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec2 texelSize = 1.0 / vec2(textureSize(uInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(uInput, coord + offset).r;
        }
    }
    fragColor = result / (4.0 * 4.0);
}
)";