#include "gi/vx/vxgi_config.h"

#include <imgui.h>

namespace vxgi {

VXGIConfig::VXGIConfig(float world_size, uint32_t voxel_resolution,
                       const Texture& radiance_map)
    : world_size(world_size),
      voxel_resolution(voxel_resolution),
      radiance_map(radiance_map.Reference()) {}

void VXGIConfig::ImGuiWindow() {
  ImGui::Begin("VXGI:");
  ImGui::Checkbox("VXGI ON", &vxgi_on);
  ImGui::DragFloat("Step Size", &step_size, 0.1f, 0.02f, 10.0f);
  ImGui::DragFloat("Diffuse Offset", &diffuse_offset, 0.1f, 0.1f, 10.0f);
  ImGui::DragFloat("Diffuse Max T", &diffuse_max_t, 0.01f, 0.01f, 5.0f);
  ImGui::DragFloat("Specular Aperture", &specular_aperture, 0.02f, 0.02f, 1.5f);
  ImGui::DragFloat("Specular Offset", &specular_offset, 0.1f, 0.1f, 10.0f);
  ImGui::DragFloat("Specular Max T", &specular_max_t, 0.01f, 0.01f, 5.0f);
  ImGui::End();
}

}  // namespace vxgi