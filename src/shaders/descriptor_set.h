#include "descriptor_set_constants.h"

layout(constant_id = 0) const int  c_layerCount   = 1;
layout(constant_id = 1) const uint c_ycbcrMask    = 0;
layout(constant_id = 2) const bool c_compositing_debug = false;
layout(constant_id = 3) const int  c_blur_layer_count = 0;

layout(constant_id = 4) const uint c_colorspaceMask = 0;
layout(constant_id = 5) const bool c_st2084Output = false;

const int colorspace_sdr = 0;
const int colorspace_scRGB = 1;
const int colorspace_pq = 2;
const int colorspace_reserved = 3;
const int colorspace_max_bits = 2;

uint get_layer_colorspace(uint layerIdx) {
    return bitfieldExtract(c_colorspaceMask, int(layerIdx) * colorspace_max_bits, colorspace_max_bits);
}

layout(binding = 0, rgba8) writeonly uniform image2D dst;
// alias
layout(binding = 0, rgba8) writeonly uniform image2D dst_luma;
layout(binding = 1, rgba8) writeonly uniform image2D dst_chroma;

layout(binding = 2) uniform sampler2D s_samplers[VKR_SAMPLER_SLOTS];
layout(binding = 3) uniform sampler2D s_ycbcr_samplers[VKR_SAMPLER_SLOTS];
