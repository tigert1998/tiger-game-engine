#ifndef COMMON_TONE_MAPPING_AND_GAMMA_CORRECTION_GLSL_
#define COMMON_TONE_MAPPING_AND_GAMMA_CORRECTION_GLSL_

vec3 ACESToneMapping(vec3 color, float adaptedLum) {
    color = min(color, vec3(1e9));

    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;

    color *= adaptedLum;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

vec3 GammaCorrection(vec3 color) {
    const float gamma = 2.2;
    return pow(color, vec3(1.0 / gamma));
}

#endif
