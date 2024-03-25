#ifndef TONE_MAPPING_BILATERAL_GRID_COMMON_GLSL_
#define TONE_MAPPING_BILATERAL_GRID_COMMON_GLSL_

const float LOG_LUMINANCE_SCALE = 16;

float LogLuminance(vec3 c) {
    float lum = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
    return clamp(log(lum) / log(2) * LOG_LUMINANCE_SCALE + 256, 0, 256);
}

#endif
