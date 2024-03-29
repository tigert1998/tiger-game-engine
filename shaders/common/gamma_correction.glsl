#ifndef COMMON_GAMMA_CORRECTION_GLSL_
#define COMMON_GAMMA_CORRECTION_GLSL_

vec3 GammaCorrection(vec3 color) {
    const float gamma = 2.2;
    return pow(color, vec3(1.0 / gamma));
}

#endif
