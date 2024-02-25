#ifndef COMMON_RAND_GLSL_
#define COMMON_RAND_GLSL_

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

#endif
