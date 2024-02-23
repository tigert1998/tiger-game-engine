#include "model/model.glsl"
#include "common/alpha_test.glsl"

layout (location = 0) out vec3 ka;
layout (location = 1) out vec3 kd;
layout (location = 2) out vec4 ksAndShininess; // for Phong
layout (location = 3) out vec3 albedo;
layout (location = 4) out vec3 metallicAndRoughnessAndAo; // for PBR
layout (location = 5) out vec3 emission;
layout (location = 6) out vec4 normalAndAlpha;
layout (location = 7) out int flag; // shared variable

void main() {
    SampleForGBuffer(
        ka, kd, ksAndShininess.xyz, ksAndShininess.w,
        // for Phong
        albedo, metallicAndRoughnessAndAo.x, metallicAndRoughnessAndAo.y, metallicAndRoughnessAndAo.z,
        // for PBR
        emission, normalAndAlpha.xyz, normalAndAlpha.w, flag
        // shared variable
    );
    AlphaTest(normalAndAlpha.w);
}
