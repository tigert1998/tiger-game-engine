#include "tone_mapping/bilateral_grid.h"

#include <glad/glad.h>
#include <imgui.h>

namespace tone_mapping {

std::vector<std::unique_ptr<Shader>> BilateralGrid::kPasses = {};

const FrameBufferObject *BilateralGrid::fbo() const { return input_fbo_.get(); }

BilateralGrid::BilateralGrid(uint32_t width, uint32_t height) {
  // vao
  glGenVertexArrays(1, &vao_);

  // compile shaders
  if (kPasses.size() == 0) {
    kPasses.push_back(std::unique_ptr<Shader>(new Shader(
        {{GL_COMPUTE_SHADER, "tone_mapping/bilateral_grid/pass_0.comp"}}, {})));
    kPasses.push_back(std::unique_ptr<Shader>(new Shader(
        {{GL_COMPUTE_SHADER, "tone_mapping/bilateral_grid/pass_1.comp"}}, {})));
    kPasses.push_back(std::unique_ptr<Shader>(new Shader(
        {{GL_COMPUTE_SHADER, "tone_mapping/bilateral_grid/pass_2.comp"}}, {})));
    kPasses.push_back(std::unique_ptr<Shader>(new Shader(
        {{GL_COMPUTE_SHADER, "tone_mapping/bilateral_grid/pass_3.comp"}}, {})));
    kPasses.push_back(Shader::ScreenSpaceShader(
        "tone_mapping/bilateral_grid/pass_4.frag", {}));
  }

  // set initial values
  scale_size_ = 16;
  scale_range_ = 16;
  alpha_ = 0.5;
  beta_ = 125;
  blend_ = 1;
  exposure_ = 1;
  set_sigma(1, 1);

  Resize(width, height);
}

BilateralGrid::~BilateralGrid() { glDeleteVertexArrays(1, &vao_); }

void BilateralGrid::Resize(uint32_t width, uint32_t height) {
  if (width_ != width || height_ != height) {
    auto texture =
        Texture(nullptr, width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
    std::vector<Texture> color_textures;
    color_textures.push_back(std::move(texture));
    Texture depth_texture(nullptr, width, height, GL_DEPTH_COMPONENT,
                          GL_DEPTH_COMPONENT, GL_FLOAT, GL_CLAMP_TO_EDGE,
                          GL_LINEAR, GL_LINEAR, {}, false);
    input_fbo_.reset(new FrameBufferObject(color_textures, depth_texture));

    width_ = width;
    height_ = height;
  }

  uint32_t grid_width = (width_ + scale_size_ - 1) / scale_size_;
  uint32_t grid_height = (height_ + scale_size_ - 1) / scale_size_;
  uint32_t grid_depth = (256 + scale_range_ - 1) / scale_range_;
  if (grid_width != grid_width_ || grid_height != grid_height_ ||
      grid_depth != grid_depth_) {
    grid_width_ = grid_width;
    grid_height_ = grid_height;
    grid_depth_ = grid_depth;
    for (int i = 0; i < 2; i++) {
      grids_[i] = Texture(nullptr, GL_TEXTURE_3D, grid_width_, grid_height_,
                          grid_depth_, GL_RG16F, GL_RG, GL_FLOAT,
                          GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST, {}, false);
    }
  }
}

void BilateralGrid::set_sigma(float sigma_size, float sigma_range) {
  if (std::abs(sigma_range - sigma_range_) < 1e-6 &&
      std::abs(sigma_size - sigma_size_) < 1e-6)
    return;

  sigma_size_ = sigma_size;
  sigma_range_ = sigma_range;

  uint32_t blur_radius_size = std::ceil(sigma_size * 3);
  uint32_t blur_radius_range = std::ceil(sigma_range * 3);
  std::vector<float> weights;
  uint32_t width = std::max(blur_radius_size, blur_radius_size) * 2 + 1;
  uint32_t height = 2;
  weights.resize(width * height, 0);

  auto compute_weights = [&](uint32_t i, uint32_t radius, float sigma) {
    float total = 0;
    for (int j = -(int)radius; j <= (int)radius; j++) {
      float val = std::exp(-0.5 * std::pow(j / sigma, 2));
      weights[(height - i - 1) * width + (j + radius)] = val;
      total += val;
    }
    for (int j = -(int)radius; j <= (int)radius; j++)
      weights[(height - i - 1) * width + (j + radius)] /= total;
  };

  compute_weights(0, blur_radius_size, sigma_size);
  compute_weights(1, blur_radius_range, sigma_range);
  weights_ = Texture(weights.data(), width, height, GL_R16F, GL_RED, GL_FLOAT,
                     GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, {}, false);
}

void BilateralGrid::Draw(const FrameBufferObject *dest_fbo) {
  // clear
  glClearTexImage(grids_[0].id(), 0, GL_RG, GL_FLOAT, nullptr);

  // pass #0
  kPasses[0]->Use();
  kPasses[0]->SetUniform<uint32_t>("uScaleSize", scale_size_);
  kPasses[0]->SetUniform<uint32_t>("uScaleRange", scale_range_);
  glBindImageTexture(0, input_fbo_->color_texture(0).id(), 0, GL_FALSE, 0,
                     GL_READ_ONLY, GL_RGBA16F);
  kPasses[0]->SetUniform<int32_t>("uInput", 0);
  glBindImageTexture(1, grids_[0].id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG16F);
  kPasses[0]->SetUniform<int32_t>("uGrid", 1);
  glDispatchCompute((width_ + 3) / 4, (height_ + 3) / 4, 1);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  // pass #1, #2, #3
  for (int i = 1; i <= 3; i++) {
    kPasses[i]->Use();
    glBindImageTexture(0, grids_[(i & 1) ^ 1].id(), 0, GL_TRUE, 0, GL_READ_ONLY,
                       GL_RG16F);
    kPasses[i]->SetUniform<int32_t>("uGrid", 0);
    glBindImageTexture(1, grids_[(i & 1) ^ 0].id(), 0, GL_TRUE, 0,
                       GL_WRITE_ONLY, GL_RG16F);
    kPasses[i]->SetUniform<int32_t>("uOutputGrid", 1);
    glBindImageTexture(2, weights_.id(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    kPasses[i]->SetUniform<int32_t>("uWeights", 2);
    uint32_t blur_radius = 0;
    if (i == 1 || i == 2) {
      blur_radius = std::ceil(sigma_size_ * 3);
    } else if (i == 3) {
      blur_radius = std::ceil(sigma_range_ * 3);
    }
    kPasses[i]->SetUniform<uint32_t>("uBlurRadius", blur_radius);
    glDispatchCompute((grid_width_ + 3) / 4, (grid_height_ + 3) / 4,
                      (grid_depth_ + 3) / 4);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  }

  // pass #4
  if (dest_fbo != nullptr) dest_fbo->Bind();
  kPasses[4]->Use();
  glBindImageTexture(0, grids_[1].id(), 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG16F);
  kPasses[4]->SetUniform<int32_t>("uGrid", 0);
  glBindImageTexture(1, input_fbo_->color_texture(0).id(), 0, GL_FALSE, 0,
                     GL_READ_ONLY, GL_RGBA16F);
  kPasses[4]->SetUniform<int32_t>("uInput", 1);
  kPasses[4]->SetUniform<uint32_t>("uScaleSize", scale_size_);
  kPasses[4]->SetUniform<uint32_t>("uScaleRange", scale_range_);
  kPasses[4]->SetUniform<float>("uAlpha", alpha_);
  kPasses[4]->SetUniform<float>("uBeta", beta_);
  kPasses[4]->SetUniform<float>("uBlend", blend_);
  kPasses[4]->SetUniform<float>("uExposure", exposure_);

  glViewport(0, 0, width_, height_);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(vao_);
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);

  if (dest_fbo != nullptr) dest_fbo->Unbind();
}

void BilateralGrid::ImGuiWindow() {
  static float sigma_size = 1, sigma_range = 1;

  ImGui::Begin("Bilateral Grid Local Tone Mapping:");
  ImGui::DragInt("Scale (Size)", &scale_size_, 1, 4, 50);
  ImGui::DragInt("Scale (Range)", &scale_range_, 1, 4, 32);
  ImGui::DragFloat("Sigma (Size)", &sigma_size, 1e-2, 0.1, 5);
  ImGui::DragFloat("Sigma (Range)", &sigma_range, 1e-2, 0.1, 5);
  ImGui::DragFloat("Alpha", &alpha_, 1e-2, 0.1, 2);
  ImGui::DragFloat("Beta", &beta_, 1, -200, 200);
  ImGui::DragFloat("Blend", &blend_, 1e-2, 0, 1);
  ImGui::DragFloat("Exposure", &exposure_, 1e-1, -8, 8);
  ImGui::End();

  set_sigma(sigma_size, sigma_range);
  Resize(width_, height_);
}

}  // namespace tone_mapping