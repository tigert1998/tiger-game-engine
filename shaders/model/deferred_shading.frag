#include "model/model.glsl"

layout (location = 0) out vec3 ka;
layout (location = 1) out vec3 kd;
layout (location = 2) out vec4 ksAndShininess; // for Phong
layout (location = 3) out vec3 albedo;
layout (location = 4) out vec3 metallicAndRoughnessAndAo; // for PBR
layout (location = 5) out vec3 normal;
layout (location = 6) out vec4 positionAndAlpha;
layout (location = 7) out int flag; // shared variable

void main() {
    SampleForGBuffer(
        ka, kd, ksAndShininess.xyz, ksAndShininess.w,
        // for Phong
        albedo, metallicAndRoughnessAndAo.x, metallicAndRoughnessAndAo.y, metallicAndRoughnessAndAo.z,
        // for PBR
        normal, positionAndAlpha.xyz, positionAndAlpha.w, flag
        // shared variable
    );
    if (positionAndAlpha.w < 0.5) {
        // punch-through method
        discard;
    }
}
