#ifndef VXGI_ACCUMULATION_GLSL_
#define VXGI_ACCUMULATION_GLSL_

vec4 Accumulate(vec4 value, vec4 newValue) {
    // this accumulation equation is different from that in the original NVIDIA paper
    // it comes from wicked engine's blog:
    // https://wickedengine.net/voxel-based-global-illumination/
    value.rgb += (1 - value.a) * newValue.rgb;
    value.a += (1 - value.a) * newValue.a;

    // the accumulation equation from the original NVIDIA paper
    // value.rgb = value.a * value.rgb + (1 - value.a) * newValue.a * newValue.rgb;
    // value.a = value.a + (1 - value.a) * newValue.a;

    return value;
}

#endif