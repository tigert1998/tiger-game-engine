#ifndef VXGI_ACCUMULATION_GLSL_
#define VXGI_ACCUMULATION_GLSL_

vec4 Accumulate(vec4 value, vec4 newValue) {
    value.rgb = value.a * value.rgb + (1 - value.a) * newValue.a * newValue.rgb;
    value.a += (1 - value.a) * newValue.a;
    return value;
}

#endif