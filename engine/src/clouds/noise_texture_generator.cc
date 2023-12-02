#include "clouds/noise_texture_generator.h"

#include <glad/glad.h>

#include <memory>

#include "shader.h"
#include "texture_manager.h"
#include "utils.h"

NoiseTextureGenerator::NoiseTextureGenerator(
    float weather_texture_perlin_frequency)
    : weather_texture_perlin_frequency_(weather_texture_perlin_frequency) {
  {
    std::unique_ptr<Shader> shader(new Shader(
        {{GL_COMPUTE_SHADER, ReadFile("shaders/clouds/perlinworley.comp")}},
        {}));
    perlin_worley_texture_id_ = TextureManager::AllocateTexture3D(
        128, 128, 128, GL_RGBA8, GL_RGBA, GL_FLOAT, true);

    shader->Use();
    glBindImageTexture(0, perlin_worley_texture_id_, 0, GL_FALSE, 0,
                       GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute(128, 128, 128);
    CheckOpenGLError();
  }
  {
    std::unique_ptr<Shader> shader(new Shader(
        {{GL_COMPUTE_SHADER, ReadFile("shaders/clouds/worley.comp")}}, {}));
    worley_texture_id_ = TextureManager::AllocateTexture3D(
        32, 32, 32, GL_RGBA8, GL_RGBA, GL_FLOAT, true);

    shader->Use();
    glBindImageTexture(0, worley_texture_id_, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA8);
    glDispatchCompute(32, 32, 32);
    CheckOpenGLError();
  }
  {
    std::unique_ptr<Shader> shader(new Shader(
        {{GL_COMPUTE_SHADER, ReadFile("shaders/clouds/weather.comp")}}, {}));
    weather_texture_id_ = TextureManager::AllocateTexture(
        1024, 1024, GL_RGBA8, GL_RGBA, GL_FLOAT, true);

    shader->Use();
    shader->SetUniform<float>("perlinFrequency",
                              weather_texture_perlin_frequency_);
    shader->SetUniform<glm::vec3>("seed", glm::vec3(0));
    glBindImageTexture(0, weather_texture_id_, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_RGBA8);
    glDispatchCompute(1024 / 16, 1024 / 16, 1);
    CheckOpenGLError();
  }
}

NoiseTextureGenerator::~NoiseTextureGenerator() {
  glDeleteTextures(1, &perlin_worley_texture_id_);
  glDeleteTextures(1, &worley_texture_id_);
  glDeleteTextures(1, &weather_texture_id_);
}