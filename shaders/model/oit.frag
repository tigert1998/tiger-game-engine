#include "model/model.glsl"

layout (r32ui) uniform uimage2D uHeadPointers;
layout (binding = 0) uniform atomic_uint uListSize;
layout (rgba32ui) uniform uimageBuffer uList;

void AppendToList(vec4 fragColor) {
    uint index = atomicCounterIncrement(uListSize);
    uint prevHead = imageAtomicExchange(uHeadPointers, ivec2(gl_FragCoord.xy), index);
    uvec4 item;
    item.x = prevHead;
    item.y = packUnorm4x8(fragColor);
    item.z = floatBitsToUint(gl_FragCoord.z);
    imageStore(uList, int(index), item);
}

void main() {
    vec4 fragColor = CalcFragColor();
    AppendToList(fragColor);
}
