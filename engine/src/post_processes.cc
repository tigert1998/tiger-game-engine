#include "post_processes.h"

#include <glad/glad.h>

#include <glm/glm.hpp>

uint32_t PostProcesses::Size() const { return post_processes_.size(); }

void PostProcesses::Enable(uint32_t index, bool enabled) {
  enabled_[index] = enabled;
}

void PostProcesses::Add(std::unique_ptr<PostProcess> post_process) {
  post_processes_.push_back(std::move(post_process));
  enabled_.push_back(true);
}

void PostProcesses::Resize(uint32_t width, uint32_t height) {
  for (const auto &post_process : post_processes_) {
    post_process->Resize(width, height);
  }
}

const FrameBufferObject *PostProcesses::fbo() const {
  for (int i = 0; i < post_processes_.size(); i++) {
    if (enabled_[i]) return post_processes_[i]->fbo();
  }
  return nullptr;
}

void PostProcesses::Draw(const FrameBufferObject *dest_fbo) {
  std::vector<PostProcess *> tmp;
  tmp.reserve(post_processes_.size());
  for (int i = 0; i < post_processes_.size(); i++) {
    if (enabled_[i]) tmp.push_back(post_processes_[i].get());
  }

  for (int i = 0; i < tmp.size(); i++) {
    auto cur_fbo = i + 1 < tmp.size() ? tmp[i + 1]->fbo() : dest_fbo;
    tmp[i]->Draw(cur_fbo);
  }
}

void PostProcesses::ImGuiWindow() {
  for (int i = 0; i < post_processes_.size(); i++) {
    if (!enabled_[i]) continue;
    post_processes_[i]->ImGuiWindow();
  }
}
