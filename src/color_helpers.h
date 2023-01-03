#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

// Colormetry helper functions for DRM, kindly taken from Weston:
// https://gitlab.freedesktop.org/wayland/weston/-/blob/main/libweston/backend-drm/kms-color.c
// Licensed under MIT.

// Josh: I changed the asserts to clamps here (going to 0, rather than 1) to deal better with
// bad EDIDs (that have 0'ed out metadata) and naughty clients.

static inline uint16_t
color_xy_to_u16(float v)
{
	//assert(v >= 0.0f);
	//assert(v <= 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

    // CTA-861-G
    // 6.9.1 Static Metadata Type 1
    // chromaticity coordinate encoding
	return (uint16_t)round(v * 50000.0f);
}

static inline uint16_t
nits_to_u16(float nits)
{
	//assert(nits >= 1.0f);
	//assert(nits <= 65535.0f);
	nits = std::clamp(nits, 0.0f, 65535.0f);

	// CTA-861-G
	// 6.9.1 Static Metadata Type 1
	// max display mastering luminance, max content light level,
	// max frame-average light level
	return (uint16_t)round(nits);
}

static inline uint16_t
nits_to_u16_dark(float nits)
{
	//assert(nits >= 0.0001f);
	//assert(nits <= 6.5535f);
	nits = std::clamp(nits, 0.0f, 6.5535f);

	// CTA-861-G
	// 6.9.1 Static Metadata Type 1
	// min display mastering luminance
	return (uint16_t)round(nits * 10000.0f);
}
