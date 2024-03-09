#version 460 core

#extension GL_ARB_bindless_texture : require

#include "material.glsl"
#include "common/alpha_test.glsl"

layout (r32ui) uniform volatile coherent uimage3D uAlbedoImage;
layout (r32ui) uniform volatile coherent uimage3D uNormalImage;
uniform uint uVoxelResolution;

layout (std430, binding = 6) buffer materialsBuffer {
    Material materials[]; // per instance
};
layout (std430, binding = 7) buffer texturesBuffer {
    sampler2D textures[];
};

in vec2 gTexCoord;
in mat3 gTBN;
flat in int gInstanceID;
flat in int gAxis;

void ImageAtomicRGBA8Avg(layout (r32ui) coherent volatile uimage3D image, ivec3 coord, vec4 value) {
    value.a /= 255.0;
    uint packedValue = packUnorm4x8(value);
    uint prevValue = 0;
    uint curValue;
    while ((curValue = imageAtomicCompSwap(image, coord, prevValue, packedValue)) != prevValue) {
        prevValue = curValue;
        vec4 curUnpackedValue = unpackUnorm4x8(curValue);
        curUnpackedValue.xyz = curUnpackedValue.xyz * (curUnpackedValue.w * 255.0);
        curUnpackedValue += value;
        curUnpackedValue.xyz /= (curUnpackedValue.w * 255.0);
        if (curUnpackedValue.w >= 1) return;
        packedValue = packUnorm4x8(curUnpackedValue);
    }
}

ivec3 GetVoxelPosition() {
    int resolution = int(uVoxelResolution);
    ivec3 position = ivec3(gl_FragCoord.x, gl_FragCoord.y, resolution * gl_FragCoord.z);
    ivec3 ret;
    if (gAxis == 0) {
	    ret.x = resolution - 1 - position.z;
		ret.z = resolution - 1 - position.x;
		ret.y = position.y;
	} else if (gAxis == 1) {
	    ret.z = resolution - 1 - position.y;
		ret.y = resolution - 1 - position.z;
		ret.x = position.x;
	} else {
	   	ret.xy = position.xy;
		ret.z = resolution - 1 - position.z;
	}
    return ret;
}

void main() {
    Material material = materials[gInstanceID];

    float alpha = 1;
    vec3 albedo = material.albedo;
    if (material.diffuseTexture >= 0) {
        vec4 sampled = texture(textures[material.diffuseTexture], gTexCoord);
        albedo = pow(sampled.rgb, vec3(2.2));
        alpha = sampled.a;
    }
    AlphaTest(alpha);

    ivec3 voxelPosition = GetVoxelPosition();
    ImageAtomicRGBA8Avg(uAlbedoImage, voxelPosition, vec4(albedo, alpha));
    ImageAtomicRGBA8Avg(uNormalImage, voxelPosition, vec4(gTBN[2] * 0.5 + 0.5, 1));
}
