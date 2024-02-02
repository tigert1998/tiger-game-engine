struct FrustumPlane {
    float normal[3];
    float distance;
};

struct Frustum {
    FrustumPlane topPlane;
    FrustumPlane bottomPlane;
    FrustumPlane rightPlane;
    FrustumPlane leftPlane;
    FrustumPlane farPlane;
    FrustumPlane nearPlane;
};
