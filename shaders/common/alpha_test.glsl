#ifndef COMMON_ALPHA_TEST_GLSL_
#define COMMON_ALPHA_TEST_GLSL_

void AlphaTest(float alpha) {
    if (alpha < 0.5) discard;
}

#endif