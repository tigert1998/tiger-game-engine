struct Material {
    int ambientTexture;
    int diffuseTexture;
    int specularTexture;
    int normalsTexture;
    int metalnessTexture;
    int diffuseRoughnessTexture;
    int ambientOcclusionTexture;
    vec3 ka, kd, ks, ke;
    float shininess;
    bool bindMetalnessAndDiffuseRoughness;
};
