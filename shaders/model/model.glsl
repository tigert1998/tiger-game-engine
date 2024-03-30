#version 460 core

#extension GL_ARB_bindless_texture : require

const float zero = 1e-6;

uniform vec3 uCameraPosition;
uniform mat4 uViewMatrix;

in vOutputs {
    vec3 position;
    vec2 texCoord;
    mat3 TBN;
    flat int instanceID;
} vOut;

#include "light_sources.glsl"
#include "material.glsl"

uniform bool uDefaultShading;
uniform bool uForcePBR;

layout (std430, binding = 6) buffer materialsBuffer {
    Material materials[]; // per instance
};
layout (std430, binding = 7) buffer texturesBuffer {
    sampler2D textures[];
};

vec3 ConvertDerivativeMapToNormalMap(vec3 normal) {
    if (normal.z > -1 + zero) return normal;
    return normalize(vec3(-normal.x, -normal.y, 1));
}

void SampleForGBuffer(
    out vec3 ka, out vec3 kd, out vec3 ks, out float shininess, // for Phong
    out vec3 albedo, out float metallic, out float roughness, out float ao, // for PBR
    out vec3 emission, out vec3 normal, out float alpha, out int flag // shared variable
) {
    if (uDefaultShading) {
        ka = vec3(0.2);
        kd = vec3(0, 0, 0.9);
        ks = vec3(0.2);
        emission = vec3(0);
        shininess = 20;
        normal = vOut.TBN[2];
        alpha = 1;
        flag = 1;
        return;
    }

    Material material = materials[vOut.instanceID];

    if (material.metalnessTexture < 0 && !uForcePBR) {
        // for Phong
        alpha = 1;

        ka = material.ka;
        kd = material.kd;
        ks = material.ks;
        emission = material.ke;

        if (material.diffuseTexture >= 0) {
            vec4 sampled = texture(textures[material.diffuseTexture], vOut.texCoord);
            kd = pow(sampled.rgb, vec3(2.2));
            alpha = sampled.a;
        }

        if (alpha <= zero) discard;

        if (material.ambientTexture >= 0) {
            ka = texture(textures[material.ambientTexture], vOut.texCoord).rgb;
        }
        if (material.specularTexture >= 0) {
            ks = texture(textures[material.specularTexture], vOut.texCoord).rgb;
        }

        shininess = material.shininess;
        flag = 1;
    } else {
        // for PBR
        alpha = 1;

        albedo = material.albedo;
        metallic = material.metallic;
        roughness = material.roughness;
        ao = 1;
        emission = material.emission;

        if (material.diffuseTexture >= 0) {
            vec4 sampled = texture(textures[material.diffuseTexture], vOut.texCoord);
            albedo = pow(sampled.rgb, vec3(2.2));
            alpha = sampled.a;
        }

        if (alpha <= zero) discard;

        if (material.bindMetalnessAndDiffuseRoughness) {
            vec2 sampled = texture(textures[material.metalnessTexture], vOut.texCoord).gb;
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
            metallic = sampled[1]; // blue
            roughness = sampled[0]; // green
        } else {
            if (material.metalnessTexture >= 0) {
                metallic = texture(textures[material.metalnessTexture], vOut.texCoord).r;
            }
            if (material.diffuseRoughnessTexture >= 0) {
                roughness = texture(textures[material.diffuseRoughnessTexture], vOut.texCoord).r;
            }
        }

        if (material.ambientOcclusionTexture >= 0) {
            ao = texture(textures[material.ambientOcclusionTexture], vOut.texCoord).r;
        }
        flag = 2;
    }

    normal = vOut.TBN[2];
    if (material.normalsTexture >= 0) {
        normal = texture(textures[material.normalsTexture], vOut.texCoord).xyz * 2 - 1;
        normal = ConvertDerivativeMapToNormalMap(normal);
        normal = normalize(vOut.TBN * normal);
    }
}

vec4 CalcFragColor() {
    vec3 ka; vec3 kd; vec3 ks; float shininess; // for Phong
    vec3 albedo; float metallic; float roughness; float ao; // for PBR
    vec3 emission; vec3 normal; float alpha; int flag; // shared variable

    SampleForGBuffer(
        ka, kd, ks, shininess, // for Phong
        albedo, metallic, roughness, ao, // for PBR
        emission, normal, alpha, flag // shared variable
    );

    vec3 position = vOut.position;

    vec3 color;

    if (flag == 1) {
        color = CalcPhongLighting(
            ka, kd, ks, emission,
            normal, uCameraPosition, position,
            shininess, uViewMatrix
        );
    } else if (flag == 2) {
        color = CalcPBRLighting(
            albedo, metallic, roughness, ao, emission, 
            normal, uCameraPosition, position, uViewMatrix
        );
    }

    return vec4(color, alpha);
}
