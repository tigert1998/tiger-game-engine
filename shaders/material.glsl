struct Material {
    int ambientTexture;
    int diffuseTexture;
    int specularTexture;
    int normalsTexture;
    int metalnessTexture;
    int diffuseRoughnessTexture;
    int ambientOcclusionTexture;
    vec4 ka, kd, ks;
    float shininess;
    bool bindMetalnessAndDiffuseRoughness;
};
