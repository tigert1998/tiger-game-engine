#version 460 core

#extension GL_ARB_bindless_texture : require

#include "material.glsl"
#include "light_sources.glsl"

layout (binding = 0, r32ui) uniform volatile coherent uimage3D uAlbedoImage;
layout (binding = 1, r32ui) uniform volatile coherent uimage3D uNormalImage;
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

vec4 ConvertRGBA8ToVec4(uint val){
    return vec4(
        float((val & 0x000000FF)),
        float ((val & 0x0000FF00) >> 8U),
        float ((val & 0x00FF0000) >> 16U),
        float ((val & 0xFF000000) >> 24U)
    );
}

uint ConvertVec4ToRGBA8(vec4 val){
    return (uint(val.w) & 0x000000FF) << 24U |
        (uint(val.z) &0x000000FF) << 16U |
        (uint(val.y) & 0x000000FF) << 8U |
        (uint(val.x) & 0x000000FF);
}

void ImageAtomicRGBA8Avg(layout (r32ui) coherent volatile uimage3D img, ivec3 coord, vec4 val) {
    val.rgb *= 255;
    uint newVal = ConvertVec4ToRGBA8(val);
    uint prevStoredVal = 0;
    uint curStoredVal;
    uint numIterations = 0;
    while ((curStoredVal = imageAtomicCompSwap(img, coord, prevStoredVal, newVal)) != prevStoredVal 
            && numIterations < 255) {
        prevStoredVal = curStoredVal;
        vec4 rval = ConvertRGBA8ToVec4(curStoredVal);
        rval.xyz = (rval.xyz * rval.w);
        vec4 curValF = rval + val;
        curValF.xyz /= (curValF.w);
        newVal = ConvertVec4ToRGBA8(curValF);
        ++numIterations;
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

    ivec3 voxelPosition = GetVoxelPosition();
    ImageAtomicRGBA8Avg(uAlbedoImage, voxelPosition, vec4(albedo, alpha));
    ImageAtomicRGBA8Avg(uNormalImage, voxelPosition, vec4(gTBN[2] * 0.5 + 0.5, 1));
}
