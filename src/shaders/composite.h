#include "colorimetry.h"

uint pseudo_random(uint seed) {
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return seed * 1664525u + 1013904223u;
}

void compositing_debug(uvec2 coord) {
    uvec2 pos = coord;
    pos.x -= (u_frameId & 2) != 0 ?  128 : 0;
    pos.y -= (u_frameId & 1) != 0 ?  128 : 0;

    if (pos.x >= 40 && pos.x < 120 && pos.y >= 40 && pos.y < 120) {
        vec4 value = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (pos.x >= 48 && pos.x < 112 && pos.y >= 48 && pos.y < 112) {
            uint random = pseudo_random(u_frameId.x + (pos.x & ~0x7) + (pos.y & ~0x7) * 50);
            vec4 time = round(unpackUnorm4x8(random)).xyzw;
            if (time.x + time.y + time.z + time.w < 2.0f)
                value = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        imageStore(dst, ivec2(coord), value);
    }
}

vec4 sampleLayer(sampler2D layerSampler, uint layerIdx, vec2 uv, bool unnormalized) {
    vec2 coord = ((uv + u_offset[layerIdx]) * u_scale[layerIdx]);
    vec2 texSize = textureSize(layerSampler, 0);

    if (coord.x < 0.0f       || coord.y < 0.0f ||
        coord.x >= texSize.x || coord.y >= texSize.y) {
        float border = (u_borderMask & (1u << layerIdx)) != 0 ? 1.0f : 0.0f;

        if (c_compositing_debug)
            return vec4(vec3(1.0f, 0.0f, 1.0f) * border, border);

        return vec4(0.0f, 0.0f, 0.0f, border);
    }

    if (!unnormalized)
        coord /= texSize;

    vec4 color = textureLod(layerSampler, coord, 0.0f);

    if (get_layer_colorspace(layerIdx) == colorspace_pq) {
        color.rgb = pqToNits(color.rgb);
        if (!c_st2084Output) {
            color.rgb = convert_primaries(color.rgb, rec2020_to_xyz, xyz_to_rec709);
            color.rgb = nitsToLinear(color.rgb);
        }
    } else if (get_layer_colorspace(layerIdx) == colorspace_scRGB) {
        if (!c_st2084Output) {
            color.rgb = nitsToLinear(color.rgb);
        }
    } else if (get_layer_colorspace(layerIdx) == colorspace_sdr) {
        if (c_st2084Output) {
            color.rgb = linearToNits(color.rgb);
        }
    }
    return color;
}

vec3 encodeOutputColor(vec3 linearOrNits) {
    if (!c_st2084Output) {
        // linearOrNits -> linear
        return linearToSrgb(linearOrNits.rgb);
    } else {
        // linearOrNits -> nits
        return nitsToPq(linearOrNits.rgb);
    }
}
