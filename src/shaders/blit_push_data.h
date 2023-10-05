layout(push_constant)
uniform layers_t {
    vec2 u_scale[VKR_MAX_LAYERS];
    vec2 u_offset[VKR_MAX_LAYERS];
    float u_opacity[VKR_MAX_LAYERS];
    uint u_borderMask;
    uint u_frameId;
    uint u_blur_radius;

    uint8_t u_shaderFilter[VKR_MAX_LAYERS];
    uint8_t u_padding[2];

    // hdr
    float u_linearToNits; // sdr -> hdr
    float u_nitsToLinear; // hdr -> sdr
    float u_itmSdrNits;
    float u_itmTargetNits;
};
