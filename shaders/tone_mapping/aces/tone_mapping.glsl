#ifndef TONE_MAPPING_ACES_TONE_MAPPING_GLSL_
#define TONE_MAPPING_ACES_TONE_MAPPING_GLSL_

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

#endif
