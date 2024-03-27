#ifndef VXGI_ACCUMULATION_GLSL_
#define VXGI_ACCUMULATION_GLSL_

vec4 Accumulate(vec4 value, vec4 newValue) {
    // this accumulation equation is different from that in the original NVIDIA paper
    // it comes from wicked engine's blog:
    // https://wickedengine.net/2017/08/30/voxel-based-global-illumination/
    value.rgb += (1 - value.a) * newValue.rgb;
    value.a += (1 - value.a) * newValue.a;
    return value;
}

#endif