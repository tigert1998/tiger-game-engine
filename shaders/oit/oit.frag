#version 420 core

out vec4 fragColor;

uniform usampler2D uHeadPointers;
uniform usamplerBuffer uList;
uniform sampler2D uBackground;
uniform sampler2D uBackgroundDepth;
uniform vec2 uScreenSize;

const int MAX_FRAGMENTS = 128;
uvec4 fragments[MAX_FRAGMENTS];

int BuildLocalFragmentList() {
    uint current = texelFetch(uHeadPointers, ivec2(gl_FragCoord.xy), 0).r;
    int count = 0;
    while (current != 0xFFFFFFFF && count < MAX_FRAGMENTS) { 
        fragments[count] = texelFetch(uList, int(current)); 
        current = fragments[count].x;
        count++;
    }
    return count;
}

void SortFragmentList(int count) {
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            float depthI = uintBitsToFloat(fragments[i].z);
            float depthJ = uintBitsToFloat(fragments[j].z);
            // from far objects to near objects
            if (depthI < depthJ) {
                uvec4 temp = fragments[i];
                fragments[i] = fragments[j];
                fragments[j] = temp;
            }
        }
    }
}

vec4 CalcFragColor(int count, out float depth) {
    vec2 coord = gl_FragCoord.xy / uScreenSize;
    vec4 fragColor = texture(uBackground, coord);
    depth = texture(uBackgroundDepth, coord).r;
    for (int i = 0; i < count; i++) {
        float tmpDepth = uintBitsToFloat(fragments[i].z);
        if (tmpDepth > depth) continue;
        depth = tmpDepth;
        vec4 color = unpackUnorm4x8(fragments[i].y);
        fragColor = mix(fragColor, color, color.a);
    }
    return fragColor;
}

void main() {
    int count = BuildLocalFragmentList();
    SortFragmentList(count);
    float depth;
    fragColor = CalcFragColor(count, depth);
    gl_FragDepth = depth;
}
